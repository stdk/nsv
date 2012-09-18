#include "net.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "log.h"

//man netdevice
//there is another way to implement this function: via getifaddrs(3)
int get_ip(const char * ifname,uint32_t *ip)
{
	int fd = socket(PF_INET,SOCK_DGRAM,0);
	if(-1 == fd) {
                xlog2("get_ip socket: %s",strerror(errno));
		return -1;
	}
	
        const size_t MAX_IFS = 3;
	char ifreq_buf[sizeof(ifreq)*MAX_IFS];
	ifconf ifconfig;
	ifconfig.ifc_len = sizeof(ifreq_buf);
	ifconfig.ifc_buf = (caddr_t)ifreq_buf;
	
        int ioctl_ret = ioctl(fd,SIOCGIFCONF,&ifconfig);

        close(fd);

        if(-1 == ioctl_ret) {
                xlog2("ioctl SIOCGIFCONF: failed");
                return -1;
	}

	
        for(size_t i = 0; i < MAX_IFS; i++)
	{
		ifreq *current = &ifconfig.ifc_req[i];
                if(0 != strcmp(ifname,current->ifr_name)) continue;
		
		sockaddr_in * addr = (sockaddr_in*)&current->ifr_addr;
                *ip = *(uint32_t*)&addr->sin_addr;
                return 0;
        }
	
	return -1;
}

static sockaddr_in prepare_sockaddr(uint32_t ip,uint16_t port)
{
    sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = *(in_addr*)&ip;

    return addr;
}

int create_connection(uint32_t ip,uint16_t port)
{
    int fd = socket(AF_INET,SOCK_STREAM,0);
    if(fd < 0) {
        xlog2("create_connection[%0X:%i]: socket: %s",ip,port,strerror(errno));
        return -1;
    }

    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr = prepare_sockaddr(ip,port);
    int ret = connect(fd,(struct sockaddr*)&addr,sizeof(addr));
    if( ret < 0 && errno != EINPROGRESS) {
        close(fd);
        xlog2("connect[%0X:%i]: %s",ip,port,strerror(errno));
        return -2;
    }

    return fd;
}
