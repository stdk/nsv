#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <limits.h>
#include <sched.h>
#include <libgen.h>
#include <time.h>
#include <signal.h>

#include "platform.sys.h"

static volatile unsigned int *cvspiregs, *cvgpioregs, *cvtwiregs;
static volatile unsigned int *cvmiscregs, *cvtimerregs;
static int last_gpio_adr = 0;

static int devmem = -1;

int platform_init()
{
    devmem = open("/dev/mem", O_RDWR|O_SYNC);
    if(devmem == -1) return -1;

    cvspiregs = (unsigned int *) mmap(0, 4096,PROT_READ | PROT_WRITE, MAP_SHARED, devmem, 0x71000000);
    cvtwiregs = cvspiregs + (0x20/sizeof(unsigned int));
    cvgpioregs = (unsigned int *) mmap(0, 4096,PROT_READ | PROT_WRITE, MAP_SHARED, devmem, 0x7c000000);

    return 0;
}

int platform_uninit()
{
    if( (munmap((void*)cvspiregs,4096) == -1) ||
        (munmap((void*)cvgpioregs,4096) == -1) ) {
        return -1;
    }

    if(close(devmem) == -1) {
        return -2;
    }

    return 0;
}

static void reservemem(void) {
        char dummy[32768];
        int pgsize;
        FILE *maps;

        pgsize = getpagesize();
        mlockall(MCL_CURRENT|MCL_FUTURE);
        for (size_t i = 0; i < sizeof(dummy); i += 4096) {
                dummy[i] = 0;
        }

        maps = fopen("/proc/self/maps", "r");
        if (maps == NULL) {
                perror("/proc/self/maps");
                exit(1);
        }
        while (!feof(maps)) {
                unsigned int s, e, i;
                char m[PATH_MAX + 1];
                char perms[16];
                int r = fscanf(maps, "%x-%x %s %*x %*x:%*x %*d",
                  &s, &e, perms);
                if (r == EOF) break;
                assert (r == 3);

                i = 0;
                while ((r = fgetc(maps)) != '\n') {
                        if (r == EOF) break;
                        m[i++] = r;
                }
                m[i] = '\0';
                assert(s <= e && (s & 0xfff) == 0);
                if (perms[0] == 'r') while (s < e) {
                        unsigned char *ptr = (unsigned char *)s;
                        *ptr;
                        s += pgsize;
                }
        }
}

static int semid = -1;
static int sbuslocked = 0;
void sbuslock(void) {
        int r;
        struct sembuf sop;
        if (semid == -1) {
                key_t semkey;
                reservemem();
                semkey = 0x75000000;
                semid = semget(semkey, 1, IPC_CREAT|IPC_EXCL|0777);
                if (semid != -1) {
                        sop.sem_num = 0;
                        sop.sem_op = 1;
                        sop.sem_flg = 0;
                        r = semop(semid, &sop, 1);
                        assert (r != -1);
                } else semid = semget(semkey, 1, 0777);
                assert (semid != -1);
        }
        sop.sem_num = 0;
        sop.sem_op = -1;
        sop.sem_flg = SEM_UNDO;
        r = semop(semid, &sop, 1);
        assert (r == 0);
        cvgpioregs[0] = (cvgpioregs[0] & ~(0x3<<15))|(last_gpio_adr<<15);
        sbuslocked = 1;

        //added - required to start communication with bus
        cvspiregs[0x64 / 4] = 0x0; // RX IRQ threahold 0
        cvspiregs[0x40 / 4] = 0x80000c02; // 24-bit mode, no byte swap
        cvspiregs[0x60 / 4] = 0x0; // 0 clock inter-transfer delay
        cvspiregs[0x6c / 4] = 0x0; // disable interrupts
        cvspiregs[0x4c / 4] = 0x4; // deassert CS#
        for (int i = 0; i < 8; i++) cvspiregs[0x58 / 4];
        last_gpio_adr = 3;
        cvgpioregs[0] = (0x3<<15|1<<3|1<<17);
}


void sbusunlock(void) {
        struct sembuf sop = { 0, 1, SEM_UNDO};
        int r;
        if (!sbuslocked) return;
        r = semop(semid, &sop, 1);
        assert (r == 0);
        sbuslocked = 0;
}


void poke8(unsigned int adr, unsigned char dat) {
        if (!cvspiregs) *(unsigned char *)adr = dat;
        else {
                assert(last_gpio_adr == 0x3);
                if (adr & 0x1) {
                        cvgpioregs[0] = (0x3<<15|1<<17);
                        poke16(adr, dat << 8);
                } else {
                        cvgpioregs[0] = (0x3<<15|1<<3);
                        poke16(adr, dat);
                }
                cvgpioregs[0] = (0x3<<15|1<<17|1<<3);
        }

}


void poke16(unsigned int adr, unsigned short dat) {
        unsigned int dummy;
        unsigned int d = dat;
        volatile unsigned int *spi = cvspiregs;
        if (last_gpio_adr != adr >> 5) {
                last_gpio_adr = adr >> 5;
                cvgpioregs[0] = (cvgpioregs[0] & ~(0x3<<15))|((adr>>5)<<15);
        }
        adr &= 0x1f;

        if (!cvspiregs) {
                *(unsigned short *)adr = dat;
        } else asm volatile (
                "mov %0, %1, lsl #18\n"
                "orr %0, %0, #0x800000\n"
                "orr %0, %0, %2, lsl #3\n"
                "3: ldr r1, [%3, #0x64]\n"
                "cmp r1, #0x0\n"
                "bne 3b\n"
                "2: str %0, [%3, #0x50]\n"
                "1: ldr r1, [%3, #0x64]\n"
                "cmp r1, #0x0\n"
                "beq 1b\n"
                "ldr %0, [%3, #0x58]\n"
                "ands r1, %0, #0x1\n"
                "moveq %0, #0x0\n"
                "beq 3b\n"
                : "+r"(dummy) : "r"(adr), "r"(d), "r"(cvspiregs) : "r1","cc"
        );
}


static inline void
poke16_stream(unsigned int adr, unsigned short *dat, unsigned int len) {
        volatile unsigned int *spi = cvspiregs;
        unsigned int cmd, ret, i, j;

        if (last_gpio_adr != adr >> 5) {
                last_gpio_adr = adr >> 5;
                cvgpioregs[0] = ((adr>>5)<<15|1<<3|1<<17);
        }
        adr &= 0x1f;

        spi[0x4c/4] = 0x0;	/* continuous CS# */
        cmd = (adr<<18) | 0x800000 | (dat[0]<<3) | (dat[1]>>13);
        do {
                spi[0x50/4] = cmd;
                cmd = dat[1]>>13;
                while (spi[0x64/4] == 0);
                ret = spi[0x58/4];
                assert (spi[0x64/4] == 0); /* */
        } while (!(ret & 0x1));

        spi[0x40/4] = 0x80000c01; /* 16 bit mode */
        i = len - 1;
        len -= 6;
        dat++;

        for (j = 0; j < 4; j++) {
                spi[0x50/4] = (dat[0]<<3) | (dat[1]>>13);
                dat++;
        }

        while (len--) {
                spi[0x50/4] = (dat[0]<<3) | (dat[1]>>13);
                dat++;
                while (spi[0x64/4] == 0);
                spi[0x58/4];
                i--;
        }

        spi[0x4c/4] = 0x4;	/* deassert CS# */
        spi[0x50/4] = dat[0]<<3;

        while (i) {
                while ((spi[0x64/4]) == 0);
                spi[0x58/4];
                i--;
        }

        spi[0x40/4] = 0x80000c02; /* 24 bit mode */
}


void poke32(unsigned int adr, unsigned int dat) {
        if (!cvspiregs) *(unsigned int *)adr = dat;
        else {
                poke16(adr, dat & 0xffff);
                poke16(adr + 2, dat >> 16);
        }
}


unsigned char peek8(unsigned int adr) {
        unsigned char ret;
        if (!cvspiregs) ret = *(unsigned char *)adr;
        else {
                unsigned short x;
                x = peek16(adr);
                if (adr & 0x1) ret = x >> 8;
                else ret = x & 0xff;
        }
        return ret;
}


unsigned short peek16(unsigned int adr) {
        unsigned short ret;
        volatile unsigned int *spi = cvspiregs;

        if (last_gpio_adr != adr >> 5) {
                last_gpio_adr = adr >> 5;
                cvgpioregs[0] = ((adr>>5)<<15|1<<3|1<<17);
        }

        adr &= 0x1f;

        if (!cvspiregs) ret = *(unsigned short *)adr;
        else asm volatile (
                "mov %0, %1, lsl #18\n"
                "2: str %0, [%2, #0x50]\n"
                "1: ldr r1, [%2, #0x64]\n"
                "cmp r1, #0x0\n"
                "beq 1b\n"
                "ldr %0, [%2, #0x58]\n"
                "ands r1, %0, #0x10000\n"
                "bicne %0, %0, #0xff0000\n"
                "moveq %0, #0x0\n"
                "beq 2b\n"
                : "+r"(ret) : "r"(adr), "r"(cvspiregs) : "r1", "cc"
        );

        return ret;
}


unsigned int peek32(unsigned int adr) {
        unsigned int ret;
        if (!cvspiregs) ret = *(unsigned int *)adr;
        else {
                unsigned short l, h;
                l = peek16(adr);
                h = peek16(adr + 2);
                ret = (l|(h<<16));
        }
        return ret;
}


static inline
void peek16_stream(unsigned int adr, unsigned short *dat, unsigned int len) {
        unsigned int dummy;
        volatile unsigned int *spi = cvspiregs;
        if (last_gpio_adr != adr >> 5) {
                last_gpio_adr = adr >> 5;
                cvgpioregs[0] = ((adr>>5)<<15|1<<3|1<<17);
        }
        adr &= 0x1f;

        asm volatile(
                "mov %0, #0x0\n"
                "str %0, [%4, #0x4c]\n"
                "mov %1, %1, lsl #18\n"
                "orr %1, %1, #(1<<15)\n"
                "2: str %1, [%4, #0x50]\n"
                "1: ldr %0, [%4, #0x64]\n"
                "cmp %0, #0x0\n"
                "beq 1b\n"
                "ldr %0, [%4, #0x58]\n"
                "tst %0, #0x10000\n"
                "beq 2b\n"
                "\n"
                "3:\n"
                "strh %0, [%3], #0x2\n"
                "mov %0, #0x80000001\n"
                "orr %0, %0, #0xc00\n"
                "str %0, [%4, #0x40]\n"
                "ldr %0, [%4, #0x40]\n"
                "str %1, [%4, #0x50]\n"
                "str %1, [%4, #0x50]\n"
                "sub %2, %2, #0x4\n"
                "4: str %1, [%4, #0x50]\n"
                "5: ldr %0, [%4, #0x64]\n"
                "cmp %0, #0x0\n"
                "beq 5b\n"
                "ldr %0, [%4, #0x58]\n"
                "subs %2, %2, #0x1\n"
                "strh %0, [%3], #0x2\n"
                "bne 4b\n"
                "\n"
                "mov %0, #0x4\n"
                "str %0, [%4, #0x4c]\n"
                "mov %1, #0x0\n"
                "str %1, [%4, #0x50]\n"
                "6: ldr %0, [%4, #0x64]\n"
                "cmp %0, #0x0\n"
                "beq 6b\n"
                "ldr %0, [%4, #0x58]\n"
                "strh %0, [%3], #0x2\n"
                "\n"
                "7: ldr %0, [%4, #0x64]\n"
                "cmp %0, #0x0\n"
                "beq 7b\n"
                "ldr %0, [%4, #0x58]\n"
                "strh %0, [%3], #0x2\n"
                "\n"
                "8: ldr %0, [%4, #0x64]\n"
                "cmp %0, #0x0\n"
                "beq 8b\n"
                "ldr %0, [%4, #0x58]\n"
                "strh %0, [%3], #0x2\n"
                "\n"
                "mov %0, #0x80000002\n"
                "orr %0, %0, #0xc00\n"
                "str %0, [%4, #0x40]\n"
                : "+r"(dummy), "+r"(adr), "+r"(len), "+r"(dat)
                : "r"(cvspiregs)
                : "cc", "memory"
        );
}


void sspi_cmd(unsigned int cmd) {
        unsigned int i;
        unsigned int s = peek16(0x66) & 0xfff0;

        // pulse CS#
        poke16(0x66, s | 0x2);
        poke16(0x66, s | 0x0);

        for (i = 0; i < 32; i++, cmd <<= 1) {
                if (cmd & 0x80) {
                        poke16(0x66, s | 0x4);
                        poke16(0x66, s | 0xc);
                } else {
                        poke16(0x66, s | 0x0);
                        poke16(0x66, s | 0x8);
                }
        }
}

