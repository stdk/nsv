#include "can_protocol.h"
#include "storage.h"
#include "can_device.h"
#include "device_container.h"

int get_stoplist_version(DC& container)
{
    xlog("get_stoplist_version");

    CANMsg msg(0,CAN_GET_ACTION_LIST_VERSION);
    container.for_each_active(TYPE_CAN,&msg);

    return 0;
}

int handle_get_stoplist_version(can_message * msg, DC& container)
{
        xlog("handle_get_stoplist_version");

        uint32_t addr = msg->id.p.addr;

        if (device_data* device = container.deviceByAddr(addr)) {
            device->stoplist = *(uint32_t*)msg->data;
        } else {
            xlog2("unknown device[%X] responded with its stiolist version",addr);
        }

        /*xlog2("CAN read:[addr %X][func %i][len %i][num %i][end %i]",msg->id.p.addr,msg->id.p.func,msg->length,msg->id.p.num,msg->id.p.end);

        char data_dump[3*CAN_MESSAGE_LENGTH+1] = {0};
        for(int i=0;i<CAN_MESSAGE_LENGTH;i++) {
            snprintf(data_dump+3*i,4,"%02hhX ",msg->data[i]);
        }

        xlog2("data[%s]",data_dump);*/

        return 0;
}

class UpdateStoplistTask : public Task
{
    DC* container;
    device_data* device;

    char* raw_data;

    struct header
    {
        uint16_t version;
        uint16_t crc;
    } *head;

    char* data;
    char* pos;
    uint32_t size_left;
    const uint8_t packets_in_group;

    //return values:
    //-1 : on error
    // 0 : when ordinary CAN_ADD_ACTION_LIST_CARD packet has been sent to device
    // 1 : when CAN_SET_ACTION_LIST_VERSION packet has been sent to device
    int next_operation()
    {
        xlog("UpdateStoplistTask::next_operation state data[%p] pos[%p] size_left[%i]",
             data,pos,size_left);

        if(size_left > 0) {
            //flash block data consists of 11 packets. Last packet in chain should be sent with id.p.end flag enabled
            CANMsg msg(device->addr,CAN_ADD_ACTION_LIST_CARD);
            msg.set_num( ( (pos - data) / CAN_MESSAGE_LENGTH ) % packets_in_group );
            if(size_left > CAN_MESSAGE_LENGTH) {
                msg.set_data(pos,CAN_MESSAGE_LENGTH);

                //we send 11 packets with 8 stoplist cards as a group
                //so, when num reaches 10, we should send end packet and repeat sequence
                msg.set_end( msg.inf.id.p.num == packets_in_group - 1 );

                pos       += CAN_MESSAGE_LENGTH;
                size_left -= CAN_MESSAGE_LENGTH;

            } else { //when there is data for only one last packet
                msg.set_data(pos,size_left);
                msg.set_end(1);

                pos += size_left;
                size_left = 0;
            }
            xlog("CAN_ADD_ACTION_LIST_CARD length[%i] num[%i] end[%i]",msg.inf.length,msg.inf.id.p.num,msg.inf.id.p.end);
            return msg.send();
        } else {
            //send delay update command - device should be updated after its reboot
            CANMsg msg(device->addr,CAN_SET_ACTION_LIST_VERSION);
            msg.set_data(head,sizeof(*head));
            msg.send();

            return 1;
        }
    }

public:
    UpdateStoplistTask(DC* _container,device_data* _device,char* _data,uint32_t size)
        :container(_container),device(_device),raw_data(_data),
         head((header*)_data),
         data(_data+sizeof(header)),pos(data),size_left(size-sizeof(header)),
         packets_in_group(11)
    {
        xlog2("UpdateStoplistTask[%X]",device->addr);
    }

    virtual ~UpdateStoplistTask() {
        xlog2("~UpdateStoplistTask");
        //enable broadcast requests
        container->endBusyTask();
        delete[] raw_data;
    }

    // -1: on error when executing next_operation
    //  0: success
    int run() {
        xlog("UpdateStoplistTask::run");

        //entering busy mode - disable broadcast requests
        container->beginBusyTask();

        container->deactivateByAddr(device->addr);

        if( CANMsg::create(device->addr,CAN_CLEAR_ACTION_LIST).send() ) {
            xlog2("UpdateStoplistTask::run cannot send stoplist clear command");
            return -1;
        }

        if( next_operation() == -1 ) {
            return -1;
        } else {
            return 0;
        }
    }

    virtual int callback(can_message * msg,DC& container) {
        uint32_t addr = msg->id.p.addr;
        uint8_t func = msg->id.p.func;

        xlog("UpdateStoplistTask::callback addr[%hX][%hhX]",addr,func);

        if(func != S_ACK) {
            xlog2("UpdateStoplistTask::callback addr[%hX][%hhX] func != S_ACK",addr,func);
            device->progress = -1;
            return -1;
        }

        int ret = next_operation();
        if(ret == 0) {
            size_t bytes_written = pos - data;
            device->progress = 100 * bytes_written / (bytes_written + size_left);
        } else if(ret == 1) {
            device->progress = 0;
            return -1; // completed
        }

        return 0;
    }
};

//return values:
// 0 : everything is ok, stoplist write process successfully started.
//-1 : there is no device with given address among active devices (either busy or absent).
//-2 : error when reading file with given filename (no such file or i/o error).
//-4 : pending task for given address had been successfully removed
//-5 : error happened during execution of first iteration of write process
int start_stoplist_write(uint32_t addr,const char* filename,DC *container)
{
    xlog2("updating stoplist on [%X] with [%s]",addr,filename);

    //remove pending tasks for given addr
    if(findTask2(addr)) {
        removeTask2(addr);
        return -4;
    }

    device_data* device = container->deviceByAddr(addr);
    if(!device) {
        xlog2("updating stoplist[%s] : device[%X] not found",filename,addr);
        return -1;
    }

    long int size = 0;
    char* data = get_file_contents(filename,&size);
    if(!data) {
        xlog2("reading file[%s] failed",filename);
        return -2;
    }

    Task* task = new UpdateStoplistTask(container,device,data,size);
    addTask2(device->addr,task);
    if( task->run() == -1) {
        removeTask2(device->addr);
        return -5;
    }

    return 0;
}

