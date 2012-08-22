#include "can.h"
#include "log.h"
#include "config.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static int can_fd = -1;

#ifdef CAN_LINCAN

#define CAN_PORT "/dev/can0"

#include "can_driver.h"
#include "canmsg.h"

void FillPacket(canmsg_t * pack, CAN_ID head)
{
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	pack->flags = MSG_EXT | MSG_LOCAL;
	pack->cob   = 0;
	pack->id    = head.id;
	pack->timestamp = tv;
	pack->length = 0;
}

int can_open()
{
	can_fd = open(CAN_PORT, O_RDWR | O_NONBLOCK);
	if(-1 == can_fd) {
		xlog("Open CAN: fail");
		return -1;
	}
	
	/* set mask */
	struct canfilt_t filter;
	filter.flags = MSG_EXT;
	filter.queid = 0;
	filter.cob = 0;
	filter.id   = 0x0;
	filter.mask = 0x10000000;//turniket mask
	if( ioctl(can_fd, CANQUE_FILTER, &filter) == -1) {
		xlog("Open CAN: mask: fail");
		return -1;
	} else {
		xlog("Open CAN: ok");
		return can_fd;
	}
}

int can_close()
{
	xlog("CAN close");
	return close(can_fd);
}

int can_write(can_message const * msg)
{
	xlog("CAN write: addr[%X] func[%i] len[%i]",msg->id.p.addr,
											   msg->id.p.func,
											   msg->length);
	
	canmsg_t pack;
	CAN_ID id = msg->id;
	
	//calculating transmission parameters
	size_t last_msg_length = msg->length % CAN_MSG_LENGTH;
	size_t last_packet = msg->length / CAN_MSG_LENGTH;
	if(0 == last_msg_length) {
		if(msg->length > 0) {
				last_msg_length = CAN_MSG_LENGTH;
				last_packet -= 1;
		}
	}
	
	int result = 0;
	
	while(id.p.num <= last_packet) {
			if(id.p.num == last_packet) {
				id.p.end = 1;
			} else {
				id.p.end = 0;
			}
		
			//prepare can packet
			FillPacket(&pack,id);
			//last packet check
			if(id.p.num == last_packet) {
				pack.length = last_msg_length;
			} else {
				pack.length = CAN_MSG_LENGTH;
			}
			//copying current piece of data to can packet data
			if(pack.length) {
				memcpy(pack.data,msg->data + id.p.num*CAN_MSG_LENGTH,pack.length);
			}
			
                        xlog2("CAN write: sending: num[%i] end[%i] len[%i]",id.p.num,
															  id.p.end,
															  pack.length);
			//sendind current command
			if( write(can_fd,&pack,sizeof(pack)) == -1) {
				result = -1;
				xlog("CAN write: sending: fail");
				break;
			}
                        xlog2("CAN write: sending: ok");
			
			/*unsigned long res = WaitSingleAnswer(addr,func);
			smartprint("SendCanData[packet %i][end %i][len %i][answer: %i]",
				id.packet.num_pack,id.packet.end_pack,pack.length,res);
			if(res==1) {
				smartprint("breaking message sending");
				fail = true;
				break;				
			}*/
			
			id.p.num++;
	}
	
	return result;
}

int can_read(can_message * msg)
{
	canmsg_t pack;
	if( read(can_fd,&pack,sizeof(pack)) == -1 ) {
		return -1;
	}
	
	memcpy(msg->data,pack.data,pack.length);
	msg->length = pack.length;
	
        msg->id.id  = pack.id;
	

	xlog("CAN read:[addr %X][func %i][len %i][num %i][end %i]",msg->id.p.addr,
															  msg->id.p.func,
															  msg->length,
															  msg->id.p.num,
															  msg->id.p.end);
	return 0;	
}

#endif

#ifdef CAN_TS7500

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#define CANCTL_HOST		"127.0.0.1"
#define	CANCTL_PORT		7552

#define CANCTL_SERVER_STOP_CMD "$START_STOP_DAEMON -x $CANCTL -K"
#define CANCTL_SERVER_START_CMD "$START_STOP_DAEMON -x $CANCTL -S -- -b 125000 -s"

#define CAN_MSG_LENGTH 8
#define FLAG_BUS_ERROR 1
#define FLAG_ERROR_PASSIVE 2
#define FLAG_DATA_OVERRUN 4
#define FLAG_ERROR_WARNING 8
#define FLAG_RTR 0x10
#define FLAG_EXT_ID 0x20
#define FLAG_LOCAL 0x40
#define FLAG_CONTROL 0x80
#define FLAG_CMD_RXEN 1

typedef unsigned char uint8;
typedef unsigned uint32;
/*
  Raw Packet Format:

  UINT8    flags:
	       control information present (reserved for future use)
	       message originates from this node (unused)
	       message has extended ID
	       remote transmission request (RTR)
	       error warning
	       data overrun
	       error passive 
               bus error
  UINT32   id
  UINT32   timestamp_seconds
  UINT32   timestamp_microseconds
  UINT8    bytes
  UINT8[8] data
 */
struct canmsg{
  uint32 flags;  // flags
  uint32 id;
  struct timeval timestamp;
  uint32 length;
  uint8  data[CAN_MSG_LENGTH];  
};
typedef struct canmsg canmsg;

int unblock(int fd)
{
	int flags = fcntl(fd,F_GETFL,0);
	if(-1 == flags) {
		xlog("fcntl F_GETFL: %s",strerror(errno));
		return -1;
	}
	if(-1 == fcntl(fd,F_SETFL,flags|O_NONBLOCK)) {
		xlog("fcntl O_NONBLOCK: %s",strerror(errno));
		return -1;
	}
	return 0;
}

int can_open() {
	system(CANCTL_SERVER_START_CMD);

	struct sockaddr_in sa;            /* Internet address struct */
	struct hostent* hen; 	       /* host-to-IP translation */
	
	hen = gethostbyname(CANCTL_HOST);
	if (!hen) {
		xlog2("gethostbyname: %s",strerror(errno));
		return -1;
	}
	
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(CANCTL_PORT);
	memcpy(&sa.sin_addr.s_addr, hen->h_addr_list[0], hen->h_length);
	
	can_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (can_fd < 0) {
		xlog2("socket: %s",strerror(errno));
		return -1;
	}
	
	if(-1 == connect(can_fd, (struct sockaddr *)&sa, sizeof(sa))) {
		xlog2("connect: %s",strerror(errno));
		return -1;
	}
	
	if(unblock(can_fd)<0) {
		return -1;
	}
	
	canmsg cmd_msg = { FLAG_CONTROL | FLAG_CMD_RXEN,0,{0,0},0};
	memset(cmd_msg.data,0,CAN_MSG_LENGTH);
		
	if(-1 == send(can_fd,&cmd_msg,sizeof(cmd_msg),0)) {
		xlog2("send: %s",strerror(errno));
		return -1;
	}
	
	return can_fd;
}

int can_close()
{
	if(-1 == shutdown(can_fd, SHUT_RDWR)) {
		xlog("shutdown: %s",strerror(errno));
		return -1;
	}
	if( -1 == close(can_fd)) {
		xlog("close: %s",strerror(errno));
		return -1;
	}
	system(CANCTL_SERVER_STOP_CMD);
	return 0;
}

int can_write(can_message const * msg)
{
	//xlog2("CAN write:[addr %X][func %i][len %i][num %i][end %i]",msg->id.p.addr,msg->id.p.func,msg->length,msg->id.p.num,msg->id.p.end);
	canmsg inner_msg = {FLAG_EXT_ID,msg->id.id,{0,0},msg->length};
	memset(inner_msg.data,0,CAN_MSG_LENGTH);
	memcpy(inner_msg.data,msg->data,CAN_MSG_LENGTH);
	if(-1 == send(can_fd,&inner_msg,sizeof(inner_msg),0)) {
		xlog2("send: %s",strerror(errno));
		return -1;
	} else {
		return 0;
	}
}

int can_read(can_message * msg)
{
	/*canmsg cmd_msg = { FLAG_CONTROL | FLAG_CMD_RXEN,0,{0,0},0};
	memset(cmd_msg.data,0,CAN_MSG_LENGTH);
		
	if(-1 == send(can_fd,&cmd_msg,sizeof(cmd_msg),0)) {
		xlog2("send: %s",strerror(errno));
		return -1;
	}*/


	canmsg inner_msg;
	int ret = recv(can_fd,&inner_msg,sizeof(inner_msg),0);
	if(ret > 0) {
		msg->id.id = inner_msg.id;
		msg->length = inner_msg.length;
		memset(msg->data,0,CAN_MSG_LENGTH);
		memcpy(msg->data,inner_msg.data,msg->length);

		//xlog2("CAN read:[addr %X][func %i][len %i][num %i][end %i]",msg->id.p.addr,msg->id.p.func,msg->length,msg->id.p.num,msg->id.p.end);
	} else {
		xlog2("recv: %s",strerror(errno));
	}
	
	return ret;
}

#endif




