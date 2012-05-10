#include "task.h"

#include <stdlib.h>
#include <vector>

void get_event_data::clear()
{
	memset(&event,0,sizeof(event));
	idx = 0;
	pos = 0;
	event_id = 0;
}

/* -------------------------------- */

static TaskMap tasks;

task_t* addTask(uint32_t addr,can_event_func action,device_data* device)
{
	xlog("addTask");
	if(!findTask(addr)) {
		task_t task;
		task.action = action;
                task.device = device;
		task.errors = 0;

                tasks.insert(std::pair<uint32_t,task_t>(addr,task));

		/*xlog2("dump at insert");
		for(TaskMap::const_iterator i = tasks.begin();i != tasks.end(); i++) {
			xlog2("task[%hX]",i->first);
		}*/
		
		return findTask(addr);
		
	} else {
		return 0;
	}
}

task_t* findTask(uint32_t addr)
{
/*	xlog2("looking for task with addr[%hX]",addr);
	for(TaskMap::const_iterator i = tasks.begin();i != tasks.end(); i++) {
			xlog2("task[%hX]",i->first);
	}*/
	
	TaskMap::iterator i = tasks.find(addr);
	if( i != tasks.end() ) {
		return &i->second;
	} else {
		return 0;
	}	
}

int removeTask(uint32_t addr)
{
	xlog("removing task[%hX]",addr);
	return tasks.erase(addr)>0;
}

struct timeout_removal {
	event ev;
	timeval tv;
        uint32_t addr;
};

void timeout_removal_callback(int fd,short event,void *arg)
{
	xlog("timeout_removal_callback");
	timeout_removal* removal = (timeout_removal*)arg;
	if(removeTask(removal->addr)) {
		xlog("removed task[%hX] after %i sec %i msec",removal->addr,removal->tv.tv_sec,removal->tv.tv_usec);
	}

        if(removeTask2(removal->addr)) {
                xlog("removed task[%hX] after %lu sec %lu msec",removal->addr,removal->tv.tv_sec,removal->tv.tv_usec);
        }

	free(removal);
}

void taskTimeoutRemove(uint32_t addr,timeval tv)
{
	xlog("taskTimeoutRemove");

	timeout_removal* removal = (timeout_removal*)malloc(sizeof(timeout_removal));
	memset(removal,0,sizeof(removal));
	removal->addr = addr;
	removal->tv = tv;
	evtimer_set(&(removal->ev),timeout_removal_callback,removal);
	evtimer_add(&(removal->ev),&(removal->tv));
}

static TaskMap2 tasks2;

Task* addTask2(uint32_t addr,Task* task)
{
    xlog("addTask2");
    if(!findTask2(addr)) {
        tasks2.insert(std::pair<uint32_t,Task* >(addr,task));
        return task;
    } else {
        return 0;
    }
}

Task* findTask2(uint32_t addr)
{
    xlog("findTask2");
    TaskMap2::iterator i = tasks2.find(addr);
    if( i != tasks2.end() ) {
            xlog("findTask2[%p]",i->second);
            return i->second;
    } else {
            xlog("findTask2[0]");
            return 0;
    }
}


int removeTask2(uint32_t addr) {
    if(Task* task = findTask2(addr)) {
        delete task;
        return tasks2.erase(addr) > 0;
    } else {
        return -1;
    }
}
