#include "can_protocol.h"
#include "storage.h"
#include "can_device.h"
#include "device_container.h"

#include "timeout.h"

#define FORCED_REMOVE_TIMEOUT 360

//return values:
//-1 : on error
// 0 : when ordinary CAN_SET_FLASH_DATA packet has been sent
// 1 : when CAN_SET_FLASH_ADDR packet has been sent
// 2 : when CAN_UPDATE_PRG packet has been send - so there are no more operations to perform
static int firmware_write_next_operation(uint16_t addr,task_t* task)
{
    firmware_update_data* state = &task->firmware_update;

    xlog("firmware_write_next_operation state data[%p] pos[%p] size_left[%i] addr_set[%i]",state->data,
          state->pos,state->size_left,state->addr_set);

    if(state->size_left > 0) {

        //before every 64-byte block we need to send CAN_SET_FLASH_ADDR packet with current flash memory address
        if(state->addr_set == 0) {
            uint32_t flash_addr = state->pos - state->data;
            xlog("set_flash_addr [%X]",flash_addr);
            CANMsg::create(addr,CAN_SET_FLASH_ADDR,&flash_addr,sizeof(flash_addr)).send();
            state->addr_set = 1;
            return 1;
        }

        //flash block data consists of 8 packets. Last packet in chain should be sent with id.p.end flag enabled
        CANMsg msg(addr,CAN_SET_FLASH_DATA);
        msg.set_num( ( (state->pos - state->data) / CAN_MESSAGE_LENGTH ) % CAN_MESSAGE_LENGTH );
        if(state->size_left > CAN_MESSAGE_LENGTH) {
            msg.set_data(state->pos,CAN_MESSAGE_LENGTH);
            msg.set_end( msg.inf.id.p.num == (CAN_MESSAGE_LENGTH - 1) );

            state->pos       += CAN_MESSAGE_LENGTH;
            state->size_left -= CAN_MESSAGE_LENGTH;

            if(msg.end()) state->addr_set = 0;//drop addr_set flag - should send CAN_SET_FLASH_ADDR next
        } else { //when there is data for only one last packet
            msg.set_data(state->pos,state->size_left);
            msg.set_end(1);

            state->pos += state->size_left;
            state->size_left = 0;
        }
        xlog("set_flash_data length[%i] num[%i] end[%i]",msg.inf.length,msg.inf.id.p.num,msg.inf.id.p.end);
        msg.send();
    } else {
        CANMsg::create(addr,CAN_DELAY_UPDATE_PRG).send();
        return 2;
    }
    return 0;
}

static void stop_firmware_write(int progress,task_t* task,DC* container)
{
    xlog2("stop_firmware_write[%i]",progress);
    task->device->progress = progress;
    free(task->firmware_update.data);
    removeTask(task->device->addr);
    container->set_can_mode(CAN_NORMAL);
}

struct stop_firmware_write_timeout_data
{
    uint32_t addr;
    uint32_t answered;
    DC *container;
};

static void stop_firmware_write_timeout(stop_firmware_write_timeout_data *data) {
    xlog2("stop_firmware_write_timeout");

    if(task_t* task = findTask(data->addr)) {
        if(task->device->answered == data->answered) {
            stop_firmware_write(-2,task,data->container);
        } else {
            xlog2("task->device->answered == data->answered");
        }
    }
}

static int handle_firmware_write_response(can_message * msg,DC& container)
{
    uint32_t addr = msg->id.p.addr;
    uint8_t func = msg->id.p.func;

    xlog("handle_firmware_write_response addr[%hX][%hhX]",addr,func);

    if(task_t* task = findTask(addr)) {

        if(func != S_ACK) {
            xlog2("handle_firmware_write_response addr[%hX][%hhX] func != S_ACK",addr,func);
            stop_firmware_write(-1,task,&container);
            return -1;
        }

        int ret = firmware_write_next_operation(addr,task);
        if(ret == 1) {
            size_t bytes_written = task->firmware_update.pos - task->firmware_update.data;
            size_t bytes_left = task->firmware_update.size_left;
            task->device->progress = 100 * bytes_written / (bytes_written + bytes_left);
        } else if(ret == 2) {
            stop_firmware_write(0,task,&container);
        }
    }

    return 0;
}

//return values:
// 0 : everything is ok, firmware update process successfully started.
//-1 : there is no device with given address among active devices (either busy or absent).
//-2 : error when reading file with given filename (no such file or i/o error).
//-3 : there is an active task for given address already, i.e. CAN packet handler
//     for this device has been registered before and haven't been removed.
//     Since most important operations temporarily remove corresponding device from
//     active device list, this code can be returned when event gathering task
//     hasn't been completed at the moment of firmware update process call.
//     Usually, -1 and -2 error codes tell us about failed attempt to execute a command on a busy device.
//     When such errors arise, upper level logic should recommend user to try again.
int start_firmware_write(uint32_t addr,const char* filename,DC *container)
{
    xlog2("updating firmware on [%X] with [%s]",addr,filename);

    device_data* device = container->deviceByAddr(addr);
    if(!device) {
        xlog2("updating firmware[%s] : device[%X] not found",filename,addr);
        return -1;
    }

    int size = 0;
    char* data = get_file_contents(filename,&size);
    if(!data) {
        xlog2("reading file[%s] failed",filename);
        return -2;
    }

    if(task_t* task = addTask(addr,handle_firmware_write_response,device))
    {
        //entering firmware update mode - disable broadcast requests
        container->set_can_mode(CAN_BUSY);
        container->deactivateByAddr(addr);

        task->firmware_update.data = data;
        task->firmware_update.pos = data;
        task->firmware_update.size_left = size;
        task->firmware_update.addr_set = 0;

        stop_firmware_write_timeout_data data = { addr,task->device->answered,container };
        timeout_remove(FORCED_REMOVE_TIMEOUT,stop_firmware_write_timeout,&data);

        firmware_write_next_operation(addr,task);

        return 0;
    } else {
        return -3;
    }
}
