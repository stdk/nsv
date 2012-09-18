#include <adbk.h>
#include <adbk_struct.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <net.h>
#include <log.h>

#include <event.h>

#define EVENTS_TO_READ_COUNT 30

struct __attribute__ ((packed)) SPacket
{
    uint32_t len;
    uint8_t type;
    void* data;
    size_t datalen; //helper
    uint16_t crc;

#define WRAP_LEN (sizeof(((SPacket*)0)->len) + sizeof(((SPacket*)0)->type) + sizeof(((SPacket*)0)->crc))

    static SPacket create(uint8_t packet_type,void* data,size_t datalen) {
        SPacket packet = { datalen + WRAP_LEN,packet_type,data,datalen,0x1234 };
        return packet;
    }

    int send(int fd) {
        const int header_len = sizeof(len)+sizeof(type);
        xlog("SPacket header len[%i] type[%i]",len,type);
        ssize_t bytes = ::send(fd,this,header_len,0);
        xlog("SPacket header send[%i]",bytes);
        if(bytes != header_len) {
            xlog2("SPacket header send: %s",strerror(errno));
            return -1;
        }

        bytes = ::send(fd,this->data,datalen,0);
        xlog("SPacket data send[%i]",bytes);
        if(bytes != (ssize_t)datalen) {
            xlog2("SPacket data send: %s",strerror(errno));
            return -2;
        }

        bytes = ::send(fd,&crc,sizeof(crc),0);
        xlog("SPacket crc send[%i]",bytes);
        if(bytes != sizeof(crc)) {
            xlog2("SPacket crc send: %s",strerror(errno));
            return -3;
        }

        return 0;
    }

    int recv(int fd,void* data,size_t full_datalen) {
        const int header_len = sizeof(len) + sizeof(type);
        ssize_t bytes = ::recv(fd,this,header_len,0);
        xlog("SPacket header recv[%i]",bytes);
        if(bytes != header_len) {
            xlog2("SPacket header recv: %s",strerror(errno));
            return -1;
        }
        xlog("len[%u] type[%i]",len,type);

        datalen = len - WRAP_LEN;
        xlog("SPacket data len[%i]",datalen);
        if(datalen > full_datalen) {
            xlog2("SPacket data recv: datalen > full_datalen[%i]",full_datalen);
            return -2;
        }

        bytes = ::recv(fd,data,datalen,0);
        xlog("SPacket data recv[%i]",bytes);
        if(bytes != (ssize_t)datalen) {
            xlog2("SPacket data recv: %s",strerror(errno));
            return -3;
        }

        bytes = ::recv(fd,&crc,sizeof(crc),0);
        xlog("SPacket crc recv[%i]",bytes);
        if(bytes != sizeof(crc)) {
            xlog2("SPacket crc recv: %s",strerror(errno));
            return -4;
        }

        if(crc != 0x1234) {
            xlog2("SPacket crc recv: != 0x1234");
            return -5;
        }

        return 0;
    }
};

struct adbk_command
{
    typedef void (*cmd_cb)(adbk_command*);

    uint32_t ip;
    DC *container;
    cmd_cb callback;

    int fd;
    event socket_ev;

    timeval timeout;
    event timeout_ev;

    char data[2048];
    size_t len;

    adbk_command(uint32_t ip,DC* container,SCommand* command,cmd_cb cb,
                 void* blocks=0,size_t blocks_length=0) {
        xlog("adbk_command[%i]",command->cmd);
        this->ip = ip;
        this->container = container;
        this->callback = cb;

        memcpy(this->data,command,sizeof(*command));
        set_data(blocks,blocks_length);
    }

    ~adbk_command() {
        xlog("~adbk_command");
        event_del(&socket_ev);
        event_del(&timeout_ev);
        close(fd);
    }

    void set_data(void* blocks,size_t length) {
        memcpy(this->data + sizeof(SCommand),blocks,length);
        this->len = sizeof(SCommand) + length;
    }

    static void timeout_callback(int fd,short event,void* arg) {
        xlog2("adbk_command timeout_callback");
        delete (adbk_command*)arg;
    }

    void set_timeout() {
        xlog("adbk_command set_timeout");

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        evtimer_set(&timeout_ev,timeout_callback,this);
        evtimer_add(&timeout_ev,&timeout);
    }

    void next_stage(void (*cb)(int,short,void*),short event) {
        event_set(&socket_ev,fd,event,cb,this);
        event_add(&socket_ev,NULL);
    }

};

static void adbk_recv_stage(int fd,short event,void *arg)
{
    xlog("adbk_recv_stage");
    adbk_command* c = (adbk_command*)arg;

    SPacket answer;
    if( answer.recv(fd,c->data,sizeof(c->data)) < 0 ) {
        xlog("adbk_recv_stage: failed");
    } else {
        c->len = answer.datalen;
        c->callback(c);
    }

    delete c;
}

static void adbk_send_stage(int fd,short event,void *arg)
{
    xlog("adbk_send_stage");
    adbk_command* c = (adbk_command*)arg;

    int ret = SPacket::create(ptCommand,c->data,c->len).send(fd);
    if(ret < 0) {
        xlog("adbk_send_stage: failed");
        delete c;
        return;
    }

    c->next_stage(adbk_recv_stage,EV_READ);
}

static int send_adbk_command(adbk_command* c)
{
    c->fd = create_connection(c->ip,ADBK_PORT);
    if(c->fd < 0) return -1;

    c->set_timeout();
    c->next_stage(adbk_send_stage,EV_WRITE);

    return 0;
}

template<int command,void (*callback)(adbk_command* cmd)>
static void execute(device_data* device,DC* container) {
    SCommand cmd = { command,0,0,btNone,0 };
    adbk_command* c = new adbk_command(device->addr,container,&cmd,callback);
    send_adbk_command(c);
}

template<void (*callback)(adbk_command* cmd),typename DataType>
static void execute(device_data* device,DC* container,SCommand *cmd,DataType* data=(char*)0) {
    adbk_command* c = new adbk_command(device->addr,container,cmd,callback);
    if(data) c->set_data(data,sizeof(DataType));
    send_adbk_command(c);
}

class ADBKService : public IDeviceProcessor
{
    DC *container;
public:
    ADBKService(DC* con):container(con) {}

    static void handle_sync_time(adbk_command* cmd) {
        xlog("handle_sync_time");
        struct __attribute__ ((packed)) sync_time_answer {
            SAnswer s;
        } *answer = (sync_time_answer*)cmd->data;

        if(answer->s.err_code != answOK) {
            return xlog2("answer[%i] != answOk",answer->s.err_code);
        }
        if(cmd->len < sizeof(*answer)) {
            return xlog2("answer[%i] < required[%i]",cmd->len,sizeof(*answer));
        }

        if(device_data* device = cmd->container->deviceByAddr(cmd->ip)) {
            //device->answer(device->addr,device->sn);
            device->inner_time = device->last_answer_time;
        } else {
            xlog2("handle_sync_time: there is no device[%X]",cmd->ip);
        }
    }

    static void handle_get_state(adbk_command* cmd) {
        xlog("handle_get_state");

        struct __attribute__ ((packed)) get_state_answer {
            SAnswer s;
            SStateBlock state;
        } *answer = (get_state_answer*)cmd->data;

        if(answer->s.err_code != answOK) {
            return xlog2("answer[%i] != answOk",answer->s.err_code);
        }
        if(cmd->len < sizeof(*answer)) {
            return xlog2("answer[%i] < required[%i]",cmd->len,sizeof(*answer));
        }

        if(device_data* device = cmd->container->deviceByAddr(cmd->ip)) {
           snprintf(device->version,sizeof(device->version),"%s%s",answer->state.sw_ver,
                                                                   answer->state.card.hw_ver+3);
           uint64_t sn = 0;
           memcpy(&sn,answer->state.card.sn,sizeof(sn));

           cmd->container->answer(device->addr,sn);
        } else {
            xlog2("handle_get_state: there is no device[%X]",cmd->ip);
        }
    }

    static void handle_get_events(adbk_command* cmd) {
        xlog("handle_get_events");

        struct get_events_answer {
            SAnswer s;
            FLASHEVENT event;
        } *answer = (get_events_answer*)cmd->data;

        if(answer->s.err_code != answOK) {
            return xlog2("answer[%i] != answOk",answer->s.err_code);
        }

        if(answer->s.block_type ==  btNone) return; //there is no data from adbk

        if(answer->s.block_type != btEvent) {
            return xlog2("block[%i] != btEvent",answer->s.block_type);
        }
        const size_t required_length = sizeof(SAnswer) + sizeof(FLASHEVENT)*answer->s.block_count;
        if(cmd->len <  required_length) {
            return xlog2("answer[%i] < required[%i]",cmd->len,required_length);
        }

        if(device_data *device = cmd->container->deviceByAddr(cmd->ip)) {
            for(int i=0;i< answer->s.block_count;i++) {
                FLASHEVENT *f = &answer->event + i;
                cmd->container->eventStorage()->save_event(device,f);
                device->current_event_id = f->EventNumber;
            }
        } else {
             xlog2("handle_get_events: there is no device[%X]",cmd->ip);
        }
    }

    static void handle_get_last_event(adbk_command* cmd) {
         xlog("handle_get_last_event");

         struct get_last_event_answer {
             SAnswer s;
             FLASHEVENT event;
         } *answer = (get_last_event_answer*)cmd->data;
         if(answer->s.err_code != answOK) {
             return xlog2("answer[%i] != answOk",answer->s.err_code);
         }
         if(cmd->len < sizeof(*answer)) {
             return xlog2("answer[%i] < required[%i]",cmd->len,sizeof(*answer));
         }

         if(device_data* device = cmd->container->deviceByAddr(cmd->ip)) {
            device->last_event_id = answer->event.EventNumber;

            SCommand getEventsCommand = {
                cmdGetEvents,
                device->current_event_id + 1,
                EVENTS_TO_READ_COUNT,
                btNone,
                0
            };
            execute<handle_get_events>(device,cmd->container,&getEventsCommand,(char*)0);
         } else {
             xlog2("handle_get_last_event: there is no device[%X]",cmd->ip);
         }
    }

    void process(device_data* device) {
        execute<cmdGetState,handle_get_state>(device,container);

        SCommand syncTimeCmd = {cmdSyncTime,0,0,btTime,1};
        CLOCKDATA time = CLOCKDATA::now();
        execute<handle_sync_time>(device,container,&syncTimeCmd,&time);

        execute<cmdGetLastEventNo,handle_get_last_event>(device,container);
    }
};

void handle_get_update(adbk_command* cmd)
{
     xlog2("handle_get_update");

     SAnswer *answer = (SAnswer*)cmd->data;

     if(answer->err_code != answOK) {

         if(device_data* device = cmd->container->deviceByAddr(cmd->ip)) {
             device->progress = answer->err_code;
         }

         return xlog2("answer[%i] != answOk",answer->err_code);
     }

     if(cmd->len < sizeof(*answer)) {
         return xlog2("answer[%i] < required[%i]",cmd->len,sizeof(*answer));
     }
}

int md5_firmware_file(const char* filename, void* dst, size_t len)
{
    const char* md5sum_format = "/bin/md5sum %s/%s";
    const char* firmware_dir = getenv("FIRMWARE_DIR");

    char popen_cmd[512] = {0};
    snprintf(popen_cmd,sizeof(popen_cmd),md5sum_format,firmware_dir,filename);

    FILE* md5 = popen(popen_cmd,"r");
    if(!md5) {
        xlog2("adbk_update_cmd popen[%s]",strerror(errno));
        return -1;
    }
    fread(dst,1,len,md5);
    fclose(md5);

    xlog2("md5[%.*s]",len,(char*)dst);

    return 0;
}

int adbk_update_cmd(char adbk_num,const char* filename,DC* container)
{
    xlog2("adbk_update_cmd[%i][%s]",adbk_num,filename);

    uint32_t server_ip = 0;
    if(-1 == get_ip("eth0",&server_ip)) return -1;

    //generate adbk ip from server ip and adbk index
    uint32_t adbk_addr = server_ip;
    ((char*)&adbk_addr)[3] = adbk_num;

    char str_adbk_addr[16] = {0};
    inet_ntop(AF_INET,&adbk_addr,str_adbk_addr,sizeof(str_adbk_addr));
    xlog2("str_adbk_addr[%s]",str_adbk_addr);

    if(device_data* device = container->deviceByAddr(adbk_addr)) {
        char str_server_ip[16] = {0};
        inet_ntop(AF_INET,&server_ip,str_server_ip,sizeof(str_server_ip));

        SCommand updateCmd = { cmdUpdate,0,0,btFile,1 };
        SFileBlock block;

        const char* firmware_url_format = getenv("FIRMWARE_URL_FORMAT");
        snprintf(block.filename,sizeof(block.filename),firmware_url_format,str_server_ip,filename);
        xlog2("adbk_update_cmd url[%s]",block.filename);

        if( md5_firmware_file(filename,block.md5,sizeof(block.md5)) < 0) {
            return -2;
        }

        /*const char* firmware_dir = getenv("FIRMWARE_DIR");
        char popen_cmd[200] = {0};
        snprintf(popen_cmd,sizeof(popen_cmd),"/bin/md5sum %s/%s",firmware_dir,filename);

        FILE* md5_file = popen(popen_cmd,"r");
        if(!md5_file) {
            xlog2("adbk_update_cmd popen[%s]",strerror(errno));
            return -3;
        }
        fread(block.md5,32,1,md5_file);
        fclose(md5_file);

        xlog2("md5[%.*s]",32,block.md5);*/

        execute<handle_get_update>(device,container,&updateCmd,&block);

        return 0;
    } else {
        xlog2("adbk_update_cmd: device not found[%s]",str_adbk_addr);
        return -2;
    }
}

int adbk_service(DC& container)
{
    xlog("adbk_service");

    ADBKService service(&container);
    container.for_each(TYPE_ADBK,&service);

    return 0;
}
