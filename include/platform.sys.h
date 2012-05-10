#ifndef PLATFORM_SYS_H
#define PLATFORM_SYS_H

extern "C" {
    int platform_init();
    int platform_uninit();

    void poke8(unsigned int, unsigned char);
    void poke16(unsigned int, unsigned short);
    void poke32(unsigned int, unsigned int);
    unsigned char peek8(unsigned int);
    unsigned short peek16(unsigned int);
    unsigned int peek32(unsigned int);
    void sbuslock(void);
    void sbusunlock(void);
}

#endif //PLATFORM_SYS_H
