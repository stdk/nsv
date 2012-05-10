#ifndef CAN_HANDLER_H
#define CAN_HANDLER_H

#include <event.h>
#include <device_container.h>

int init_can_handler(event *can_event,DC * container);
int uninit_can_handler(event *can_event);

#endif //CAN_HANDLER_H
