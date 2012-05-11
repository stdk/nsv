#include "task.h"

#include <stdlib.h>
#include <vector>

struct timeout_removal {
	event ev;
	timeval tv;
        uint32_t addr;
};

void timeout_removal_callback(int fd,short event,void *arg)
{
	xlog("timeout_removal_callback");
	timeout_removal* removal = (timeout_removal*)arg;

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
