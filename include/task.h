#ifndef TASK_H
#define TASK_H

#include <map>
#include <memory>
#include <event.h>

#include "can_device.h"
#include "can.h"
//#include "struct.h"
#include "log.h"
#include "device_container.h"

typedef int (*can_event_func)(can_message * msg,DC& container);

//#define PACK_STRUCTURES

#ifdef PACK_STRUCTURES
#define PACKED __attribute__((__packed__))
#else
#define PACKED
#endif

class Task
{
public:
     virtual ~Task() {}
     virtual int run() = 0;
     virtual int callback(can_message * msg,DC& container) = 0;
};

typedef std::map<uint32_t,Task* > TaskMap2;

Task* addTask2(uint32_t addr,Task* task);
Task* findTask2(uint32_t addr);
int removeTask2(uint32_t addr);

void taskTimeoutRemove(uint32_t addr,timeval tv);

#endif //TASK_H
