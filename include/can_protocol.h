#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "can.h"
#include "task.h"
#include "can_device.h"
#include "device_container.h"

/* ------------------------------------------------- */
#define CAN_MASTER_ID			0x8000

#define S_ACK					1
#define S_NACK					2
#define CAN_GET_DEVICE_TYPE		3
#define CAN_GET_SN			4
#define CAN_GET_VERSION			5
#define CAN_SET_FLASH_ADDR		6
#define CAN_SET_FLASH_DATA		7
#define CAN_UPDATE_PRG			8
#define CAN_ADD_GROUP			9
#define CAN_DEL_GROUP			10
#define CAN_SHOW_ADDR			11
#define CAN_GET_TIME			12
#define CAN_DELAY_UPDATE_PRG		13
#define CAN_GET_EVENT			14
#define CAN_LAST_EVENT_ID		15
#define CAN_GET_ARTICLE			16
#define CAN_SET_ARTICLE			17
#define CAN_REPEAT_COMMAND		18
#define CAN_ADD_ACTION_LIST_CARD	19
#define CAN_SET_ACTION_LIST_VERSION	20
#define CAN_GET_ACTION_LIST_VERSION	21
#define CAN_CLEAR_ACTION_LIST		22
#define CAN_SET_TIME			25
#define CAN_GET_LIST_BAD_CONTRACT	32
#define CAN_SET_CONTRACT_STATUS		33
#define CAN_SET_CONTRACT_RANGE		34
#define CAN_SET_SALE_CASH_LIMIT		35
#define CAN_GET_SALE_CASH_LIMIT		36
#define CAN_SHOW_MESSAGE			37 // development message
#define CAN_VALIDITY_CHECK		38 // first command of validation process
#define CAN_VALIDITY_SUCCESS	39 // second command of validation process
#define CAN_FLASHEVENTS_CLEAR	40
#define CAN_READ_EPROM				51
#define CAN_WRITE_EPROM				52

#define CAN_SET_TIME_MSG_LENGTH		6

/* ------------------------------------------------- */

class CANMsg : public IDeviceProcessor
{
public:
	CANMsg(unsigned short addr,unsigned char func,void * data = 0,size_t len = 0);
	static CANMsg create(unsigned short addr,unsigned char func,void * data = 0,size_t len = 0);
	int send();
        void process(device_data* device);
        void set_data(void* data,size_t len);
        void set_num(uint8_t num);
        void set_end(uint8_t num);

        uint8_t num();
        uint8_t end();
	//
	can_message inf;
};

/* ------------------------------------------------- */
// common manipulation over CAN

int start_firmware_write(uint32_t addr,const char* filename,DC *container);
int start_stoplist_write(uint32_t addr,const char* filename,DC *container);

/* ------------------------------------------------- */
// CAN command initiators

int get_sn(DC& container);
int get_last_event_id(DC& container);
int get_version(DC& container);
int get_time(DC& container);
int set_time(DC& container);
int get_events(DC& container);
int get_stoplist_version(DC& container);

/* ------------------------------------------------- */
// incoming CAN message handlers

// possible prototypes:
// f(device_data* device,can_message * msg,DC& container)
// f(device_data* device,can_message * msg,DC& container)

int redirect(can_message * msg,DC& container);
int handle_get_sn(can_message * msg,DC& container);
int handle_get_version(can_message * msg,DC& container);
int handle_get_time(can_message * msg,DC& container);
int handle_get_event(can_message * msg,DC& container);
int handle_get_last_event_id(can_message * msg,DC& container);
int handle_get_stoplist_version(can_message * msg, DC& container);

/* ------------------------------------------------- */

#endif //PROTOCOL_H
