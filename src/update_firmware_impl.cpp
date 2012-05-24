#include "can_protocol.h"
#include "storage.h"
#include "can_device.h"
#include "device_container.h"

class UpdateFirmwareTask : public Task
{
    DC* container;
    device_data* device;
    char* data;
    char* pos;
    uint32_t size_left;
    uint32_t addr_set;

    //return values:
    //-1 : on error
    // 0 : when ordinary CAN_SET_FLASH_DATA packet has been sent to device
    // 1 : when CAN_SET_FLASH_ADDR packet has been sent to device
    // 2 : when CAN_UPDATE_PRG packet has been sent, so there are no more operations to perform
    int next_operation()
    {
        xlog("firmware_write_next_operation state data[%p] pos[%p] size_left[%i] addr_set[%i]",
             data,pos,size_left,addr_set);

        if(size_left > 0) {
            //before every 64-byte block we need to send CAN_SET_FLASH_ADDR packet with current flash memory address
            if(addr_set == 0) {
                xlog("set_flash_addr [%X]",flash_addr);
                uint32_t flash_addr = pos - data;                
                CANMsg::create(device->addr,CAN_SET_FLASH_ADDR,&flash_addr,sizeof(flash_addr)).send();
                addr_set = 1;
                return 1;
            }

            //flash block data consists of 8 packets. Last packet in chain should be sent with id.p.end flag enabled
            CANMsg msg(device->addr,CAN_SET_FLASH_DATA);
            msg.set_num( ( (pos - data) / CAN_MESSAGE_LENGTH ) % CAN_MESSAGE_LENGTH );
            if(size_left > CAN_MESSAGE_LENGTH) {
                msg.set_data(pos,CAN_MESSAGE_LENGTH);
                msg.set_end( msg.inf.id.p.num == (CAN_MESSAGE_LENGTH - 1) );

                pos       += CAN_MESSAGE_LENGTH;
                size_left -= CAN_MESSAGE_LENGTH;

                if(msg.end()) addr_set = 0;//drop addr_set flag - should send CAN_SET_FLASH_ADDR next
            } else { //when there is data for only one last packet
                msg.set_data(pos,size_left);
                msg.set_end(1);

                pos += size_left;
                size_left = 0;
            }
            xlog("set_flash_data length[%i] num[%i] end[%i]",msg.inf.length,msg.inf.id.p.num,msg.inf.id.p.end);
            return msg.send();
        } else {
            //send delay update command - device should be updated after its reboot
            CANMsg::create(device->addr,CAN_DELAY_UPDATE_PRG).send();
            return 2;
        }
    }

public:
    UpdateFirmwareTask(DC* _container,device_data* _device,char* _data,uint32_t size)
        :container(_container),device(_device),data(_data),pos(_data),size_left(size),addr_set(0)
    {
        xlog2("UpdateFirmwareTask[%X]",device->addr);
    }

    virtual ~UpdateFirmwareTask() {
        xlog2("~UpdateFirmwareTask");
        //enable broadcast requests
        container->endBusyTask();
        delete[] data;
    }

    // -1: on error when executing next_operation
    //  0: success
    int run() {
        xlog("UpdateFirmwareTask::run");

        //entering firmware update mode - disable broadcast requests
        container->beginBusyTask();

        container->deactivateByAddr(device->addr);

        if( next_operation() == -1 ) {
            return -1;
        } else {
            return 0;
        }
    }

    virtual int callback(can_message * msg,DC& container) {
        uint32_t addr = msg->id.p.addr;
        uint8_t func = msg->id.p.func;

        xlog("UpdateFirmwareTask::callback addr[%hX][%hhX]",addr,func);

        if(func != S_ACK) {
            xlog2("handle_firmware_write_response addr[%hX][%hhX] func != S_ACK",addr,func);
            device->progress = -1;
            return -1;
        }

        int ret = next_operation();
        if(ret == 1) {
            size_t bytes_written = pos - data;
            device->progress = 100 * bytes_written / (bytes_written + size_left);
        } else if(ret == 2) {
            device->progress = 0;
            return -1; // completed
        }

        return 0;
    }
};

//return values:
// 0 : everything is ok, firmware update process successfully started.
//-1 : there is no device with given address among active devices (either busy or absent).
//-2 : error when reading file with given filename (no such file or i/o error).
//-3 : (currently unused)
//     there is an active task for given address already, i.e. CAN packet handler
//     for this device has been registered before and haven't been removed.
//     Since most important operations temporarily remove corresponding device from
//     active device list, this code can be returned when event gathering task
//     hasn't been completed at the moment of firmware update process call.
//     Usually, -1 and -2 error codes tell us about failed attempt to execute a command on a busy device.
//     When such errors arise, upper level logic should recommend user to try again.
//-4 : pending task for given address had been successfully removed
//-5 : error happened during execution of first iteration of firmware write
int start_firmware_write(uint32_t addr,const char* filename,DC *container)
{
    xlog2("updating firmware on [%X] with [%s]",addr,filename);

    //remove pending tasks for given addr
    if(findTask2(addr)) {
        removeTask2(addr);
        return -4;
    }

    device_data* device = container->deviceByAddr(addr);
    if(!device) {
        xlog2("updating firmware[%s] : device[%X] not found",filename,addr);
        return -1;
    }

    long int size = 0;
    char* data = get_file_contents(filename,&size);
    if(!data) {
        xlog2("reading file[%s] failed",filename);
        return -2;
    }

    Task* task = new UpdateFirmwareTask(container,device,data,size);
    addTask2(device->addr,task);
    if( task->run() == -1) {
        removeTask2(device->addr);
        return -5;
    }

    return 0;
}
