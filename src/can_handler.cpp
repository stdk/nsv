#include "can_handler.h"

#include <event.h>
#include <evhttp.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <can_protocol.h>
#include <storage.h>
#include <http.h>
#include <device_container.h>
#include <timed_event.h>
#include <can_handler.h>
#include <platform.h>
#include <config.h>
#include <log.h>
#include <algorithm>

typedef int (*can_event_func)(can_message * msg,DC& container);

struct can_event_data
{
        int func;
        can_event_func action;
};

static int task_based_handler(can_message * msg,DC& container) {
    xlog("task_based_handler");
    uint32_t addr = msg->id.p.addr;
    if(Task* task = findTask2(addr)) {
        if(-1 == task->callback(msg,container)) {
            xlog("got -1: removing task");
            removeTask2(addr);
        }
    } else {
        xlog("there is no task for addr[%X]",addr);
    }
    return 0;
}

//should be sorted
static const can_event_data canEvents[] = {
        { S_ACK, redirect },                                          // 1
        { S_NACK, redirect },                                         // 2
        { CAN_GET_SN, handle_get_sn },                                // 4
        { CAN_GET_VERSION, handle_get_version },                      // 5
        { CAN_GET_TIME, handle_get_time },                            // 12
        { CAN_GET_EVENT, task_based_handler },                        // 14
        { CAN_LAST_EVENT_ID, handle_get_last_event_id },              // 15
        { CAN_GET_ACTION_LIST_VERSION, handle_get_stoplist_version }, // 21
};
const uint32_t CanEventsSize = sizeof(canEvents)/sizeof(can_event_data);
inline bool operator<(const can_event_data& a,const can_event_data& b) { return a.func < b.func; }

/* ---------------------------------------- */

static void can_read_callback(int fd,short event,void *arg)
{
        platform::red_led_switch();

        DC& container = *(DC*)arg;

        can_message msg;

#ifdef CAN_READ_ALL
        while(can_read(&msg) != -1) {
#else
        if(can_read(&msg) == -1) return;
#endif

        can_event_data key = {msg.id.p.func, 0};
        xlog("got can_message [%i]",msg.id.p.func);
        const can_event_data* appropriate_event = std::lower_bound(&canEvents[0],&canEvents[CanEventsSize],key);
        if(appropriate_event) {
                xlog("calling action for [%i][%p]",appropriate_event->func,appropriate_event->action);
                appropriate_event->action(&msg,container);
        } else {
                xlog2("unknown function[%i] from device[%X]",msg.id.p.func,msg.id.p.addr);
        }

#ifdef CAN_READ_ALL
        }
#endif
}

int init_can_handler(event *can_event,DC * container)
{
    xlog2("init_can_handler: ");
    int can_fd = can_open();
    if(-1 == can_fd) {
        xlog2("can open: fail");
        return -1;
    } else {
        xlog2("can open: success");
    }

    event_set(can_event,can_fd,EV_READ|EV_PERSIST,can_read_callback,container);
    event_add(can_event,NULL);
    return 0;
}

int uninit_can_handler(event *can_event)
{
    xlog2("uninit_can_handler");
    event_del(can_event);
    return can_close();
}
