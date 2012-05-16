#ifndef ADBK_H
#define ADBK_H

#include <device_container.h>

int adbk_service(DC& container);
int adbk_update_cmd(char adbk_num,const char* filename,DC* container);

#endif //ADBK_H
