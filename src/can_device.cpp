#include "can_device.h"

#include <time.h>
#include <string.h>

#include "log.h"
#include "defines.h"

#define INACTIVE_TIMEOUT  20

void can_device_data::set_time(char * data/*[CAN_MESSAGE_LENGTH]*/)
{
	struct tm in_time;
	in_time.tm_mday = btoh(data[0]);
	in_time.tm_mon = btoh(data[1])-1;
	in_time.tm_year = btoh(data[2])+100;
	in_time.tm_hour = btoh(data[3]);
	in_time.tm_min = btoh(data[4]);
	in_time.tm_sec = btoh(data[5]);
	inner_time = mktime(&in_time);	
}

void can_device_data::get_time(char * data/*[CAN_MESSAGE_LENGTH]*/,struct tm * t)
{
	data[0] = htob(t->tm_mday);
	data[1] = htob((UCHAR)(t->tm_mon+1));
	data[2] = htob((UCHAR)(t->tm_year-100));
	data[3] = htob(t->tm_hour);
	data[4] = htob(t->tm_min);
	data[5] = htob(t->tm_sec);
}

void device_data::error(uint32_t event_id)
{
	errors += 1;
        xlog2("bad event[%i] from device[%X][%llX]",event_id,addr,sn);
}

void device_data::answer(uint32_t n_addr,uint64_t n_sn)
{
        xlog("answer [%X][%llX]",n_addr,n_sn);

	if(addr != n_addr || sn != n_sn) {
                xlog2("device[%X] answer called with wrong addr[%X] or sn[%llX]",addr,n_addr,n_sn);
		return;
	}

	time( &last_answer_time );
	answered += 1;
}

int device_data::active() const
{
    return (abs(time(0)) - last_answer_time) < INACTIVE_TIMEOUT;
}

int device_data::new_last_event_id(uint32_t new_id)
{
    //int diff = new_id - last_event_id;
    //if(last_event_id != 0 && diff > 50000) return -1;
    last_event_id = new_id;
    return 0;
}
