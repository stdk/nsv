#ifndef CAN_H
#define CAN_H

#include <stddef.h>

#define CAN_MESSAGE_LENGTH 8 

#pragma pack(1)
typedef struct _CAN_COMMAND_DATA
{
//  unsigned char               reserved  : 3;
  unsigned char         end  : 1;
  unsigned char         num  : 4;
  unsigned char         func : 8;
  unsigned short        addr : 16;
} CAN_COMMAND_DATA;

#pragma pack(1)
typedef union _CAN_ID
{
  CAN_COMMAND_DATA      p;
  unsigned int          id : 29;
} CAN_ID;

typedef struct _can_message
{
	CAN_ID id;
	char data[CAN_MESSAGE_LENGTH];
	size_t length;
} can_message;

/* low-level CAN fucntions */
/* all function return -1 on error */
int can_open();
int can_write(can_message const * msg);
int can_read(can_message * msg);
int can_close();

#endif
