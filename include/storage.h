#ifndef STORAGE_H
#define STORAGE_H

#include "can_device.h"
#include "device_container.h"
#include <stdint.h>
#include <sys/types.h>

IEventStorage* createEventStorage();

struct IDeviceContainer
{
        virtual void add_device(uint32_t addr,uint64_t sn,uint32_t event_id)=0;
        virtual ~IDeviceContainer() {}
};

int load_devices(IDeviceContainer* adder);
int save_devices(DC& container);

char* get_file_contents(const char* name,int *size);

#endif //STORAGE_H
