#ifndef HTTP_H
#define HTTP_H

#include <sys/queue.h> // MUST be included BEFORE event.h and evhttp.h
#include <event.h>
#include <evhttp.h> //MUST be declared in header to allow evhttp work correctly

#include "can_device.h"
#include "storage.h"

#include "device_container.h"

struct http_config
{
        DC * container;
        char ip[20];
};

int setup_http_server(event_base * ev_base,http_config * config);

#endif //HTTP_H
