#ifndef DEFINES_H
#define DEFINES_H

#include <stdint.h>

#define CAN_MASTER_ID			0x8000

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

#define VALID_REQUEST_PHRASE	"CHEK"
#define VALID_SUCCESS_PHRASE	"PASS"

// ****************  MASTER COMMANDS
#define MASTER_ENUM			1
#define MASTER_GETTIME			2
#define MASTER_SHOWADDR			3
#define MASTER_SENDFILE			4
#define MASTER_WRITEFLASH		5
#define MASTER_BUSY			6
#define MASTER_SETDEVLIST		7
#define MASTER_GETACTLIST		8
#define MASTER_SETACTLIST		9
#define MASTER_LAST_TRANSACT		10
#define MASTER_GET_TRANSACT		11
#define MASTER_GET_FILE			12
#define MASTER_SET_TIME			13
#define MASTER_SHUTDOWN			14
#define MASTER_GET_VERSION		15
#define MASTER_CLEAR_GROUPS		16
#define MASTER_ADD_2_GROUP		17
#define MASTER_ACTIVATE_GROUP		18
#define MASTER_SET_CONTRACT_RANGE	19
#define MASTER_GET_CONTRACTS_STATE	20
#define MASTER_SCAN_TCP_DEVICES		21
#define MASTER_SCAN_DEVICES		22
#define MASTER_REBOOT			23
#define MASTER_DEL_DEVICE		24
#define MASTER_SETJANEPROM		25
#define MASTER_CAN_SHOW_MESSAGE 26
#define MASTER_VALIDATE	27
#define MASTER_CAN_SET_LAST_EVENT_NUMBER 28
#define MASTER_CAN_FLASHEVENTS_CLEAR	29

// ***************  DEVICE TYPE
#define dtUKSBK				1
#define dtADBK				2
#define dtARM				3
#define dtPKA				4
#define dtARMK				5
//#define dtEMPTY			6
#define dtAKP				7
#define dtINIT_ARM			12
#define TYPES_COUNT			dtINIT_ARM

#define FILE_EVENTS_COUNT		500000
#define DEVICE_EVENTS_COUNT		10000
#define EVENTS_IN_ITERATION		20
//#define TRANS_COUNT_IN_MESSAGE  	50
#define NAME_MAX_LEN			255
#define MD5SUM_LEN			32

// *********** File names
#define FILE_DEVICES		"dev_array.bin"
#define FILE_EVENTS		"../sv/events.bin"
#define FILE_DEV_ID		"ids_dev.txt"
#define FILE_DEV_ID_BAK		"ids_dev.bak"

// *********** File names
#define FILE_EVENTS_CASH		"../sv/events_cash.bin"
#define FILE_EVENTS_CASH_COUNT	1000

// *********** Share Memory
#define TIME_MEM_KEY    	99
#define FLASH_MEM_KEY		100

#define SEG_SIZE ((size_t) (sizeof(CAN_DEV_DATA) * MAX_DEVICE_COUNT)) + sizeof(ULONG) * 4 + 1
#define FLASH_MEM_SIZE		4

#define BLOCK_LEN		64
#define MAX_DEVICE_COUNT	64
#define CARDPACKET		8
#define CARDDATALEN		11
#define BUF_LEN			512

#define TRUE  	true
#define FALSE 	false

#define S_OK			0
#define S_FALSE 		1
//#define S_NEXT		2

#define S_ACK			1
#define S_NACK			2


typedef unsigned char 	UCHAR;
typedef unsigned long 	DWORD;
typedef unsigned short 	WORD;
typedef unsigned short 	USHORT;
typedef bool		BOOL;
typedef unsigned long  	ULONG;
typedef uint64_t 	UINT64;

#define CLR(a,b) ((a) &= ~(1<<(b)))
#define SET(a,b) ((a) |= (1<<(b)))

// ��� ��������� �������
#define htob(h) (((h/10)<<4) + ((h%10)&0x0F))

// ��� ������ �������
#define btoh(b) ((((b&0xF0)>>4) *10) + (b&0x0F))
     

// Error code
#define SUCCESS 		0 // �������� ���������� ����������
#define MANY_NOT_ENOUGH 	1 // ����������� �������� ���������
#define TIME_NOT_ENOUGH 	2 // ��������   ����� 䳿 ��������� 
#define BAD_CARD		3 // �������� ������
#define READING_ERROR		4 // ������� ����������
#define WRITING_ERROR		5 // ������� ������ (����������� ����� ������)
#define BAD_STATUS		6 // �������� ������ (�����������)
#define TIMEOUT_ERROR		7 // ������� ��������
#define BAD_CONTRACT		8 // �������� ���������

// device not give up event
#define CRITICAL_ERRORS_COUNT	100

#endif // DEFINES_H
