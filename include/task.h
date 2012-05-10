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

struct get_event_data {
	void clear();
	FLASHEVENT event;
	uint32_t event_id;
	uint16_t idx;
	uint16_t pos;
} PACKED;

struct firmware_update_data {
    char* data;
    char* pos;
    uint32_t size_left;
    uint32_t addr_set;
};

struct task_t {
	can_event_func action;
        device_data* device;
	uint32_t errors;
	union {
		//this union should contain all possible task data variants
		get_event_data get_event;		
                firmware_update_data firmware_update;
	};
} PACKED;

class Task
{
public:
     virtual ~Task() {}
     virtual int run() = 0;
     virtual int callback(can_message * msg,DC& container) = 0;
};

typedef std::map<uint32_t,task_t> TaskMap;

typedef std::map<uint32_t,Task* > TaskMap2;



Task* addTask2(uint32_t addr,Task* task);
Task* findTask2(uint32_t addr);
int removeTask2(uint32_t addr);

task_t* addTask(uint32_t addr,can_event_func action,device_data* device=0);
task_t* findTask(uint32_t addr);
int removeTask(uint32_t addr);

void taskTimeoutRemove(uint32_t addr,timeval tv);

#endif //TASK_H
