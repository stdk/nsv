#include "http.h"
#include "log.h"
#include "config.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <task.h>
#include <can_protocol.h>
#include <adbk.h>
#include <storage.h>
#include <net.h>

#define SUCCEDED		0
#define UNSPECIFIED		1
#define UNKNOWN			2
#define INCORRECT		3
#define FAILED			4
#define DEVICE_NOT_FOUND        5
#define DEVICE_BUSY             6
#define IO_ERROR                7
#define CANCELED                8

const char* const Errors[] = {
        "Command succeded", 			// 0
        "Command not specified",  		// 1
        "Unknown command",			// 2
        "Incorrect command arguments",	        // 3
        "Command failed",			// 4
        "Device not found or inactive",         // 5
        "Device busy",                          // 6
        "Input/ouput error"                     // 7
};
#define evb_print_err(evb,err) evbuffer_add_printf((evb),"%i [%s]\n",(err),Errors[(err)])

//platform-specific realtime clock set command
#ifdef PLATFORM_TS7500
static const char* date_cmd = "/bin/date";
static const char* setrtc_cmd = "/sbin/ts7500ctl";
static char* const setrtc_args[] = {"ts7500ctl","--setrtc",0};
#define SET_RTC "ts7500ctl --setrtc" 
#elif PLATFORM_TS7260
static const char* date_cmd = "/bin/date";
static const char* setrtc_cmd = "hwclock";
static char* const setrtc_args[] = {"hwclock","-w",0};
#define SET_RTC "hwclock -w"
#endif

//Args requirements:
//The first argument, by convention, should point to the filename associated with the file being executed.
//The list of arguments must be terminated  by a NULL pointer
int execute_file(const char* path,char* const * args)
{
    pid_t pid = fork();
    if(pid == -1) { //error
        xlog2("execute_file fork: %s",strerror(errno));
        return -1;
    } else if(pid == 0) { //fork succesed and we are in child process now
        int ret = execv(path,args);
        if(ret == -1) {
            xlog2("execute_file execv: %s",strerror(errno));
            exit(1); //since we are in child, we should terminate now useless process
        }
        return 0; //on success this won't return anyway
    } else { // fork succeded and we are in parent process now (got child pid)
        xlog2("execute_file fork: child pid[%i]",pid);
        return 0;
    }
}

static int set_time(evkeyvalq * get,evkeyvalq * post,DC* container)
{
		if(const char * time = evhttp_find_header(post,"time")) {
                        save_devices(*container);

                        char date_buffer[12] = {0};
                        strncpy(date_buffer,time,sizeof(date_buffer)-1);
                        char* const date_args[] = {"date",date_buffer,0};

                        if(execute_file(date_cmd,date_args) == -1) {
                            return FAILED;
                        }

                        if(execute_file(setrtc_cmd,setrtc_args) == -1) {
                            return FAILED;
                        }

                        return SUCCEDED;
		}
		return FAILED;
}

static inline const char* get_base_filename(const char* filename)
{
    const char* last_slash = strrchr(filename,'/');
    if(last_slash) {
        xlog("/+1");
        return last_slash + 1;
    } else {
        xlog("same");
        return filename;
    }
}

static int firmware_update(uint32_t addr,const char* base_filename,const char* full_filename,DC* container)
{
    char* addr_bytes = (char*)&addr;
    if(addr_bytes[1] == 2) {
        int ret = adbk_update_cmd(addr_bytes[0],base_filename,container);
        switch(ret) {
        case -1: return FAILED;
        case  0: return SUCCEDED;
        default: return FAILED;
        }
    } else {
        int ret = start_firmware_write(addr,full_filename,container);
        switch(ret) {
        case -4: return CANCELED;
        case -3: return DEVICE_BUSY;
        case -2: return IO_ERROR;
        case -1: return DEVICE_NOT_FOUND;
        case  0: return SUCCEDED;
        default: return FAILED;
        }
    }
}

static int stoplist_update(uint32_t addr,const char* base_filename,const char* full_filename,DC* container)
{
    int ret = start_stoplist_write(addr,full_filename,container);
    switch(ret) {
    case -4: return CANCELED;
    case -3: return DEVICE_BUSY;
    case -2: return IO_ERROR;
    case -1: return DEVICE_NOT_FOUND;
    case  0: return SUCCEDED;
    default: return FAILED;
    }
}

static int upload(evkeyvalq * get,evkeyvalq * post,DC* container)
{
    const char* str_addr = evhttp_find_header(post,"addr");
    const char* filename = evhttp_find_header(post,"filename");
    const char* type = evhttp_find_header(post,"type");

    xlog2("cmd[upload]: filename [%s]",filename);

    if(str_addr && filename && type) {
        const char* base_filename = get_base_filename(filename);
        xlog2("cmd[upload]: base_filename[%s][%p]",base_filename,base_filename);
        if(!*base_filename) {
            return INCORRECT;
        }

        char* firmware_dir = getenv("FIRMWARE_DIR");
        char full_filename[200]={0};
        snprintf(full_filename,sizeof(full_filename)-1,"%s/%s",firmware_dir,base_filename);

        uint32_t addr = 0;
        if( sscanf(str_addr,"%X",&addr) == 1 ) { //correctly parsed 1 item

            const char *firmware = "firmware";
            if(strncmp(type,firmware,sizeof(firmware)) == 0) {
                return firmware_update(addr,base_filename,full_filename,container);
            }

            const char *stoplist = "stoplist";
            if(strncmp(type,stoplist,sizeof(stoplist)) == 0) {
                return stoplist_update(addr,base_filename,full_filename,container);
            }

            return INCORRECT;
        } else {
            xlog2("cmd[upload]: sscanf addr scanning failed");
            return INCORRECT;
        }
    } else {
        return INCORRECT;
    }
}

static int add_adbk(evkeyvalq * get,evkeyvalq * post,DC* container)
{
    const char* str_addr = evhttp_find_header(post,"addr");
    xlog2("cmd[add_adbk]: addr[%s]",str_addr);

    if(str_addr) {
        uint32_t addr = 0;

        if( inet_aton(str_addr,(in_addr*)&addr) ) { //non-zero - address is valid
            container->answer(addr,addr);
            return SUCCEDED;
        } else {
            xlog2("cmd[add_adbk]: invalid adbk address[%s]",str_addr);
            return INCORRECT;
        }
    }

    return FAILED;
}

static int remove_device(evkeyvalq * get, evkeyvalq * post,DC* container)
{
    const char* str_sn = evhttp_find_header(post,"sn");

    if(str_sn) {
        uint64_t sn = 0;
        if( sscanf(str_sn,"%llX",&sn) != 1 ) {
            xlog2("cmd[remove_device]: sn scanning failed");
            return INCORRECT;
        }

        if( device_data* device = container->deviceBySN(sn) ) {
            //currently, there is no way to determine a connection between task and device exactly
            //since tasks are identified only by their device addr, not by sn
            //TODO: refactoring of task-device system required
            removeTask2(device->addr); //any tasks pending for this device should be canceled
        } else {
            return DEVICE_NOT_FOUND;
        }

        int ret = container->removeBySN(sn);
        if(ret == 0) {
            return SUCCEDED;
        } else {
            return FAILED;
        }
    } else {
        return INCORRECT;
    }
}

static int set_can_mode(evkeyvalq * get, evkeyvalq * post,DC* container)
{
    const char* str_mode = evhttp_find_header(get,"mode");

    if(str_mode) {
        int mode = 0;
        if( sscanf(str_mode,"%i",&mode) != 1) {
            xlog2("cmd[set_can_mode]: mode scanning failed");
            return INCORRECT;
        }

        switch(mode) {
        case CAN_NORMAL:
        case CAN_BUSY:
            container->setCanMode(mode);
            return SUCCEDED;
        default:
            xlog2("cmd[set_can_mode]: got wrong mode[%i]",mode);
            return FAILED;
        }
    } else {
        return INCORRECT;
    }
}

typedef int (*cmd_cb)(evkeyvalq * get,evkeyvalq * post,DC* container);

static const struct command
{
	const char* name;
	cmd_cb action;
} Commands[] = {
                { "set-time"         ,set_time },           // (string time)
                { "upload"           ,upload },             // (hex addr,string filename)
                { "remove-device"    ,remove_device },      // (hex sn)
                { "add-adbk"         ,add_adbk },
                { "set-can-mode"     ,set_can_mode },
};
const int CommandsSize = sizeof(Commands)/sizeof(command);

static cmd_cb find_command(const char* name)
{
        xlog2("looking for command name[%s]",name);
	for(int i = 0;i<CommandsSize;i++) {
		if(0 == strcmp(name,Commands[i].name)) return Commands[i].action;
	}
	xlog2("command name[%s] action not found",name);
	return 0;
}

void http_cmd(evhttp_request *request,void *ctx)
{
	xlog2("http_cmd request: %s:%d URI: %s",request->remote_host,request->remote_port,request->uri);	
	
        http_config *cfg = (http_config*)ctx;
        DC* container = cfg->container;

	evbuffer *evb = evbuffer_new();
	
	evkeyvalq get;
	evhttp_parse_query(request->uri,&get);
	const char* cmd_name = evhttp_find_header(&get,"name");
	if(!cmd_name) {
		evb_print_err(evb,UNSPECIFIED);
	} else {
		if(cmd_cb action = find_command(cmd_name)) {
			evkeyvalq post;
			if(size_t size = EVBUFFER_LENGTH(request->input_buffer)) {

                                char * post_buf = (char*)malloc(size+2);//with first ? and last \0
				post_buf[0]='?';//must be present to make evhttp_parse_query happy
				post_buf[size+1]='\0'; 
				memcpy(post_buf+1,EVBUFFER_DATA(request->input_buffer),size);

				evhttp_parse_query(post_buf,&post);
				free(post_buf);
			}

                        int result = action(&get,&post,container);
                        evb_print_err(evb,result);

                        evhttp_clear_headers(&post);
		} else {
			evb_print_err(evb,UNKNOWN);
		}		
        }
        evhttp_clear_headers(&get);
	
	evhttp_add_header(request->output_headers,"Content-Type","text/plain");
	evhttp_send_reply(request,HTTP_OK,"Cmd",evb);
	evbuffer_free(evb);
}
