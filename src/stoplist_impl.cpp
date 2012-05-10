#include "can_protocol.h"
#include "storage.h"
#include "can_device.h"
#include "device_container.h"

int get_stoplist_version(DC& container)
{
    xlog("get_stoplist_version");

    /*
    CANMsg msg(0,CAN_GET_ACTION_LIST_VERSION);
    container.for_each_active(&msg);
    */
    return 0;
}

int handle_get_stoplist_version(can_message * msg, DC& container)
{
        /*xlog2("handle_get_stoplist_version");

        xlog2("CAN read:[addr %X][func %i][len %i][num %i][end %i]",msg->id.p.addr,msg->id.p.func,msg->length,msg->id.p.num,msg->id.p.end);

        char data_dump[3*CAN_MESSAGE_LENGTH+1] = {0};
        for(int i=0;i<CAN_MESSAGE_LENGTH;i++) {
            snprintf(data_dump+3*i,4,"%02hhX ",msg->data[i]);
        }

        xlog2("data[%s]",data_dump);*/

        return 0;
}
