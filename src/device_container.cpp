#include "device_container.h"

#include "storage.h"

static device_data* add_device_to_storage(DeviceStorage &storage,uint32_t addr,uint64_t sn,uint32_t event_id)
{
        xlog2("add_device_to_storage[%X][%llX][%X]",addr,sn,event_id);

        device_data device;
        memset(&device,0,sizeof(device_data));
	
	device.addr = addr;
	device.sn = sn;
	device.current_event_id = event_id;

	storage[sn] = device;

	return &storage[sn];
}

class Adder : public IDeviceContainer
{
	DeviceStorage &storage;
        ActiveDeviceStorage &active;
public:
        Adder(DeviceStorage& _storage,ActiveDeviceStorage& _active):storage(_storage),active(_active) {}

        virtual void add_device(uint32_t addr,uint64_t sn,uint32_t event_id) {
                 active[addr] = add_device_to_storage(storage,addr,sn,event_id);
	}
};

DC::DC():event_storage( std::auto_ptr<IEventStorage>(createEventStorage()) )
{
    can_mode = CAN_NORMAL;

    Adder adder(storage,active);
    load_devices(&adder);

    //event_storage = createEventStorage();
}

DC::~DC()
{
     xlog2("~DC");
}

uint32_t DC::get_can_mode()
{
    return can_mode;
}

void DC::set_can_mode(uint32_t mode)
{
    can_mode = mode;
}

void DC::deactivateByAddr(uint32_t addr)
{
    active.erase(addr);
}

int DC::removeBySN(uint64_t sn)
{
    device_data* device = deviceBySN(sn);
    if(device) {
        deactivateByAddr(device->addr);
        storage.erase(sn);
        return 0;
    } else {
        return -1;
    }
}

IEventStorage* DC::eventStorage()
{
    return event_storage.get();
}

int DC::state()
{
        xlog2("DC::state");
        if(event_storage.get()) {
            xlog2("event_storage +");
            return 0;
        } else {
            xlog2("event_storage -");
            return 1;
        }
}

uint32_t DC::activeCount() const
{
    return active.size();
}

uint32_t DC::storageCount() const
{
    return storage.size();
}

device_data* DC::deviceBySN(uint64_t sn)
{
        xlog("DC::deviceBySN[%llX]",sn);
	DeviceStorage::iterator i = storage.find(sn);
	if(i != storage.end()) {
                xlog("return &i->second;");
		return &i->second;
	} else {
                xlog("return 0;");
		return 0;
	}
}

device_data* DC::deviceByAddr(uint32_t addr)
{
        xlog("DC::deviceByAddr");
	ActiveDeviceStorage::iterator i = active.find(addr);
	if(i != active.end()) {
		return i->second;
	} else {
		return 0;
	}
}

void DC::answer(uint32_t addr,uint64_t sn)
{
        xlog("DC::answer");
        if(device_data* device = deviceByAddr(addr)) {
                xlog("deviceByAddr[%p]",device);
		if(device->sn == sn) {
                        xlog("device->sn == sn");
			device->answer(addr,sn);
			return;
                } else if(device->get_type() == TYPE_ADBK) {
                    xlog2("adbk device[%i] change sn from[%llX] to[%llX]",device->addr,device->sn,sn);

                    uint64_t previous_sn = device->sn;
                    device->sn = sn;
                    storage[sn] = *device;
                    active[addr] = &storage[sn];
                    storage.erase(previous_sn);

                    return;
                } else {
                        xlog("device->sn != sn");
			active.erase(addr);
		}
	}

        if(device_data* device = deviceBySN(sn)) {
                xlog("deviceBySn[%p]",device);
                active.erase(device->addr);
                device->addr = addr;
		active[addr] = device;
	} else {
		active[addr] = add_device_to_storage(storage,addr,sn,0);		
	}

        xlog("active[addr]->answer(addr,sn);");
	active[addr]->answer(addr,sn);
}

void DC::for_each(int type,IDeviceProcessor * processor)
{
    xlog("for_each[%i]",type);
    for(DeviceStorage::iterator i = storage.begin();i != storage.end(); i++) {
        device_data* device = &i->second;
        int device_type = device->get_type();
        xlog("device[%X] type[%i]",device->addr,device_type);
        if(type & device_type) processor->process(device);
    }
}

void DC::for_each_active(int type,IDeviceProcessor * processor,bool check)
{
    xlog("for_each_active[%i][%i]",type,check ? 1 : 0);
    for(ActiveDeviceStorage::iterator i = active.begin();i != active.end();) {
        ActiveDeviceStorage::iterator current = i++;
        device_data* device = current->second;
        int device_type = device->get_type();
        if(type & device_type) {
            xlog("device[%X] type[%i]",device->addr,device_type);
            if(check && !device->active()) {
                active.erase(current);
            } else {
                processor->process(device);
            }
        }
    }
}



