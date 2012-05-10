#ifndef NET_H
#define NET_H

#include <stdint.h>

int get_ip(const char * ifname,uint32_t *ip);
int create_connection(uint32_t addr,uint16_t port);

#endif //NET_H
