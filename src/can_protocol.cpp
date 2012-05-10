#include "can_protocol.h"
#include "can.h"
#include "log.h"
#include "storage.h"
#include "task.h"

#include <algorithm>

#define TIME_DIFF_LIMIT	5

CANMsg::CANMsg(unsigned short addr, unsigned char func, void * data, size_t len) {
	inf.id.p.addr = addr;
	inf.id.p.func = func;
	inf.id.p.num = 0;
        inf.id.p.end = 1;
        set_data(data,len);
}

void CANMsg::set_data(void* data,size_t len)
{
    memcpy(inf.data,data,len);
    inf.length = len;
}

void CANMsg::set_num(uint8_t num)
{
    inf.id.p.num = num;
}

void CANMsg::set_end(uint8_t end)
{
    inf.id.p.end = end;
}

uint8_t CANMsg::num()
{
    return inf.id.p.num;
}

uint8_t CANMsg::end()
{
    return inf.id.p.end;
}

CANMsg CANMsg::create(unsigned short addr, unsigned char func, void * data,size_t len) {
	CANMsg msg(addr, func, data, len);
	return msg;
}

int CANMsg::send() {
	return can_write(&inf);
}

void CANMsg::process(device_data* device) {
	inf.id.p.addr = device->addr;
	send();
}

/* ------------------------------------------------------------- */
int get_sn(DC& container) {
	xlog("get_sn");

        uint32_t can_mode = container.get_can_mode();

        if(CAN_NORMAL == can_mode) {
            // in normal mode we can freely send anything to CAN
            // so get_sn opearation will try to discover any devices in network
            // using broadcast message
            return CANMsg::create(CAN_MASTER_ID,CAN_GET_SN).send();
        } else if(CAN_BUSY == can_mode) {
            CANMsg msg(0,CAN_GET_SN);
            container.for_each_active(TYPE_CAN,&msg);
            return 0;
        } else {
            return -1;
        }
}

int get_last_event_id(DC& container) {
        xlog("get_last_event_id");

        CANMsg msg(0,CAN_LAST_EVENT_ID);
        container.for_each_active(TYPE_CAN,&msg,true);

        return 0;
}

int get_version(DC& container) {
	xlog("get_version");

        CANMsg msg(0,CAN_GET_VERSION);
        container.for_each_active(TYPE_CAN,&msg);

        return 0;
}

int get_time(DC& container) {
	xlog("get time");

        CANMsg msg(0,CAN_GET_TIME);
        container.for_each_active(TYPE_CAN,&msg);

        return 0;
}

int handle_set_time(can_message * msg,DC& container)
{
    xlog2("handle_set_time");

    uint16_t addr = msg->id.p.addr;

    if (container.deviceByAddr(addr)) {
            xlog2("device[%X] set_time answer [%hhX]",addr,msg->id.p.func);
    } else {
            xlog2("unknown device[%X] responded to set_time query",addr);
    }

    if( !removeTask(addr) ) {
        xlog2("removing task for addr[%hX] failed",addr);
    }

    return 0;
}

class Timesetter : public IDeviceProcessor
{
	time_t time_now;
	struct tm * tm_now;
public:
	Timesetter(time_t _time_now):time_now(_time_now) {
		tm_now = localtime(&time_now);		
	}
        void process(device_data* device) {
		if(abs(time_now - device->inner_time) < TIME_DIFF_LIMIT) {
                        xlog("device[%hX] time difference < TIME_DIFF_LIMIT",device->addr);
			return;
		}
                xlog("setting current time to device[%hX]",device->addr);

		CANMsg msg(device->addr,CAN_SET_TIME);
		can_device_data::get_time(msg.inf.data,tm_now);
		msg.inf.length = CAN_SET_TIME_MSG_LENGTH;
		msg.send();
	}
};

int set_time(DC& container) {
        xlog2("set_time");

	Timesetter timesetter(time(0));
        container.for_each_active(TYPE_CAN,&timesetter);

	return 0;
}

/* ------------------------------------------------------------- */

int redirect(can_message * msg, DC& container) {
	if (task_t* task = findTask(msg->id.p.addr)) {
                xlog("redirecting device[%X] func[%i]",msg->id.p.addr,msg->id.p.func);
		return task->action(msg, container);
	} else {
            if(Task* task = findTask2(msg->id.p.addr)) {
                if(-1 == task->callback(msg,container)) {
                    xlog("got -1: removing task");
                    removeTask2(msg->id.p.addr);
                    return -1;
                }
                return 0;
            } else {
                xlog2("no task for device[%X] to redirect func[%i]",msg->id.p.addr,msg->id.p.func);
		return -1;
            }
	}
}

int handle_get_sn(can_message * msg, DC& container) {
        xlog("handle_get_sn");

	uint16_t addr = msg->id.p.addr;
	uint64_t sn;
	memcpy(&sn, msg->data, msg->length);
	
        xlog("addr[%hX] num[%i] end[%i] sn[%llX]",addr,msg->id.p.num,msg->id.p.end,sn);

	container.answer(addr,sn);

	return 0;
}

int handle_get_last_event_id(can_message * msg,DC& container) {
	xlog("handle_get_last_event_id");

	uint16_t addr = msg->id.p.addr;

        if (device_data* device = container.deviceByAddr(addr)) {
		uint32_t new_last_event_id = *(uint32_t*)msg->data;

		device->new_last_event_id(new_last_event_id);

		xlog("device[%hX] last_event_id[%u]",addr,device->last_event_id);
	} else {
		xlog2("Unknown device[%hX] responded with its last event id",addr);
	}

	return 0;
}

int handle_get_version(can_message * msg, DC& container) {
	xlog("handle_get_version");

	uint16_t addr = msg->id.p.addr;

        if (device_data* device = container.deviceByAddr(addr)) {
		memset(device->version, 0, sizeof(device->version));
		memcpy(device->version, msg->data, msg->length);

	} else {
		xlog2("Unknown device[%hX] responded with its version",addr);
	}

	return 0;
}

int handle_get_time(can_message * msg, DC& container) {
	xlog("handle_get_time");

	uint16_t addr = msg->id.p.addr;
        if (can_device_data* device = (can_device_data*)container.deviceByAddr(addr)) {
		device->set_time(msg->data);
	} else {
		xlog2("Unknown device[%hX] responded with its time",addr);
	}

        return 0;
}
