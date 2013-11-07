#include "can_protocol.h"
#include "can.h"
#include "storage.h"
#include "task.h"
#include "device_container.h"
#include "log.h"

#include <algorithm>

#define DEVICE_EVENT_CASH	10000
#define ERROR_LIMIT	5

typedef uint32_t send_command_func(device_data* data);

//sends correct CAN_GET_EVENT command into CAN network
// returns:
//  0 when all events had been read already or error occured during command sending
//  those different situations are treated same, because reaction to them should usually be same.
//  Any specific error handling related to CAN-network stability (or CAN backend stability
//  if we are using ts7553 where CAN interaction is done via network connection to local CAN server)
//  should be performed on low level.
// -1 when send command failed
// any other return values greater than zero should be treated as event number we retrieving now
static uint32_t send_get_event_command(device_data* device)
{
	xlog("send_get_event_command");
	if(device->current_event_id < device->last_event_id) {
		xlog("data->current_event_id < data->last_event_id");
		uint32_t req_id = device->current_event_id + 1;
		int send_ret = CANMsg::create(device->addr,CAN_GET_EVENT,&req_id,sizeof(req_id)).send();
                xlog("seng_get_event_command[%i]",send_ret);
                return send_ret == -1 ? 0 : req_id;
	} else {
                xlog("send_get_event_command[current_event_id >= last_event_id]");
		return 0;
	}
}

class GetEventsTask : public Task
{
    device_data* device;
    int counter;
    uint32_t errors;

    uint32_t event_id;
    uint16_t idx;
    uint16_t pos;
    FLASHEVENT event;

    void clearEventData()
    {
        event_id = idx = pos = 0;
        memset(&event,0,sizeof(event));
    }

    uint32_t handle_nack()
    {
        xlog("NACK");
        if(errors < ERROR_LIMIT) {
            errors += 1;
            device->error(event_id);
        } else {
            errors = 0;
            device->current_event_id += 1;
        }
        return continue_task();
    }

    int continue_task()
    {
        uint32_t new_id = send_get_event_command(device);
        if(new_id == 0) {
            xlog2("cannot continue task device[%X]",device->addr);
            return -1;
        } else {
            clearEventData();
            event_id = new_id;
            return 0;
        }
    }

public:
    GetEventsTask(device_data* _device):device(_device),counter(0),errors(0) {
        clearEventData();
        xlog("GetEventsTask[%X]",device->addr);
    }

    virtual ~GetEventsTask() {
        xlog("~GetEventsTask[%i]",counter);
    }

    int run() {
        if(uint32_t new_id = send_get_event_command(device)) {
            event_id = new_id;
            /* init timeout event */
            timeval remove_timeout = {1,0};
            taskTimeoutRemove(device->addr,remove_timeout);
            return 0;
        } else {
            return -1;
        }
    }

    virtual int callback(can_message * msg,DC& container) {
        xlog("GetEventsTask::exec");
        ++counter;

        uint32_t addr = msg->id.p.addr;

        if(addr != device->addr) {
            xlog2("Wrong addr: expected[%X] got[%X]",device->addr,addr);
            return -1;
        }

        //got NACK - try to restart with the same event id
        if(msg->id.p.func == S_NACK) return handle_nack();

        int num = msg->id.p.num;
        if(idx++ != num) {
            xlog2("received packet with incorrect num[%i]",num);
            return -1;
        }

        if(num > 9) {
            xlog2("packet num[%i] > 9 : incorrect",num);
            return -1;
        }

        //here we assume that packet is correct enough
        char* data_pos = (char*)&event + pos;
        if(data_pos + msg->length > (char*)&event + sizeof(event)) {
            xlog2("possible event buffer overflow on packet[%i] with len[%i]", num, msg->length);
            device->current_event_id += 1;
            xlog2("skipping event number for device[%X] to [%i]", device->addr, device->current_event_id);
            return -1;
        }

        memcpy(data_pos,msg->data,msg->length);
        pos += msg->length;

        if(msg->id.p.end) {
            if(-1 == container.eventStorage()->save_event(device,&event)) {
                return -1;
            } else {
                xlog("task for[%X] completed event with id[%u] len[%u]",device->addr,event.EventNumber,pos);
                device->current_event_id = event.EventNumber;
                return continue_task();
            }
        }

        return 0;
    }

};

class GetEventSender : public IDeviceProcessor
{
public:
    void process(device_data* device) {
        xlog("GetEventSender");
        if(device->last_event_id > device->current_event_id) {
                //check and amend difference between current and last event id if exists
                if(device->last_event_id - device->current_event_id > DEVICE_EVENT_CASH) {
                        device->current_event_id = device->last_event_id - DEVICE_EVENT_CASH;
                }

                if(!findTask2(device->addr)) {
                    Task* task = new GetEventsTask(device);
                    addTask2(device->addr,task);
                    task->run();
                }
        }
    }
};

int get_events(DC& container)
{
        xlog("get_events");

        GetEventSender sender;
        container.for_each_active(TYPE_CAN,&sender);

        return 0;
}
