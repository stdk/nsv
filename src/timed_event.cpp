#include "timed_event.h"
#include "log.h"

static void timed_event_callback(int fd,short event,void *arg)
{
	timed_event *e = (timed_event*)arg;
	DC& container = *(DC*)(e->arg);
	
        switch(e->type) {
                case NORMAL_EVENT:
                    event_add(&e->ev,&e->interval);
                    e->action(container);
                break;
                case COUNTER_EVENT:
			if(e->interval.tv_sec <= e->interval.tv_usec++) {
				e->action(container);
				e->interval.tv_usec = 0;
			}
		break;
		default:
                        xlog2("Incorrect timed_event type[%i]",e->type);
	}
}

void event_set_timed_events(timed_event* timed_events,uint32_t len,DC* container)
{
	for(size_t i=0;i<len;i++) {
		timed_event* e = &timed_events[i];
		e->arg = container;
		
		evtimer_set(&e->ev,timed_event_callback,e);
	}
}

void event_add_timed_events(timed_event* timed_events,uint32_t len,int type)
{
	for(size_t i=0;i<len;i++) {
		timed_event* e = &timed_events[i];
                e->type = type;
                event_add(&e->ev,&e->start);
	}
}
