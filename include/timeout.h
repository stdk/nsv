#ifndef TIMEOUT_H
#define TIMEOUT_H

#include <event.h>
#include "log.h"

template<typename T>
struct timeout_action
{
    void (*callback)(T *arg);
    T arg;
    timeval t;
    event ev;
};

template<typename T>
void timeout_remove_callback(int,short,void *arg) {
    xlog2("timeout_remove_callback");
    timeout_action<T> *action = (timeout_action<T>*)arg;
    action->callback(&action->arg);
    delete action;
}

template<typename T>
void timeout_remove(int sec,void (*callback)(T*),T *arg) {
    xlog2("timeout_remove[%i]",sec);
    timeout_action<T> *action = new timeout_action<T>();
    action->callback = callback;
    action->t.tv_sec = sec;
    action->t.tv_usec = 0;
    memcpy(&action->arg,arg,sizeof(*arg));

    evtimer_set(&action->ev,timeout_remove_callback<T>,action);\
    event_add(&action->ev,&action->t);
}

#endif
