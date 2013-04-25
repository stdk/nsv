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

#define EVENTS_TO_READ_COUNT 123

#define VALID_CRC 0x1234

const timeval ADBK_TIMEOUT = { 2, 0 };

struct __attribute__((packed)) SCommandExt
{
  SCommand command;
  uint8_t data[0];

  void init(SCommand* command, void* blocks,size_t blocks_length) {
      this->command = *command;
      memcpy(this->data,blocks,blocks_length);
  }
};

struct __attribute__((packed)) SAnswerExt
{
  SAnswer answer;
  uint8_t data[0];
};

#define WRAP_LEN (sizeof(((SPacket*)0)->len) + sizeof(((SPacket*)0)->type) + sizeof(((SPacket*)0)->crc))
struct __attribute__ ((packed)) SPacket
{
    uint32_t len;
    uint8_t type;
    uint8_t data[8192];
    size_t datalen; //helper
    size_t datalen_left; //helper
    uint16_t crc;

    int init(uint8_t packet_type,SCommand* command,void* blocks,size_t blocks_length) {
        this->datalen = sizeof(*command) + blocks_length;
        if(this->datalen > sizeof(this->data)) {
            this->len = 0;
            return -1;
        }

        SCommandExt* ext = (SCommandExt*)this->data;
        ext->init(command,blocks,blocks_length);

        this->len = this->datalen + WRAP_LEN;
        this->type = packet_type;
        this->crc = VALID_CRC;
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

    int recv_header(int fd) {
        const int header_len = sizeof(len) + sizeof(type);
        ssize_t bytes = ::recv(fd,this,header_len,0);
        xlog("SPacket header recv[%i]",bytes);
        if(bytes != header_len) {
            xlog2("SPacket header recv: %s",strerror(errno));
            return -1;
        }
        xlog("recv_header len[%u] type[%i]",len,type);

        datalen = len - WRAP_LEN;
        datalen_left = datalen;
        xlog("SPacket data len[%i] left[%i]",datalen,datalen_left);

        if(datalen > sizeof(this->data)) {
            xlog2("datalen[%i] > sizeof(this->data)",datalen);
            return -2;
        }

        return 0;
    }

    int recv_data(int fd) {
        uint8_t *data_current = data + datalen - datalen_left;
        ssize_t bytes = ::recv(fd,data_current,datalen_left,0);
        xlog("SPacket data recv[%i] out of[%i/%i]",bytes,datalen_left,datalen);
        if(bytes < 0) {
            xlog2("SPacket data recv: %s",strerror(errno));
            return -3;
        }

        datalen_left -= bytes;
        if(!datalen_left) {
            bytes = ::recv(fd,&crc,sizeof(crc),0);
            xlog("SPacket crc recv[%i]",bytes);
            if(bytes != sizeof(crc)) {
                xlog2("SPacket crc recv: %s",strerror(errno));
                return -4;
            }

            if(crc != VALID_CRC) {
                xlog2("SPacket crc recv: != %X",VALID_CRC);
                return -5;
            }

            return 0;
        }

        return 1;
    }
};

struct IADBKCommandResult
{
    virtual uint32_t ip()=0;
    virtual DC* dc()=0;
    virtual device_data* device()=0;
    virtual const SPacket* packet()=0;
    virtual ~IADBKCommandResult() {}
};

struct adbk_command : public IADBKCommandResult
{
    typedef void (*cmd_cb)(IADBKCommandResult*);

    uint32_t m_ip;
    DC *m_container;
    cmd_cb callback;

    int fd;
    event socket_ev;

    timeval timeout;
    event timeout_ev;

    SPacket m_packet;

    virtual uint32_t ip() {
        return m_ip;
    }

    virtual DC* dc() {
        return m_container;
    }

    virtual device_data* device() {
        return m_container->deviceByAddr(m_ip);
    }


    virtual const SPacket* packet() {
        return &m_packet;
    }

    adbk_command(uint32_t ip,DC* container,SCommand* command,cmd_cb cb,
                 void* blocks=0,size_t blocks_length=0) {
        xlog("adbk_command[%i]",command->cmd);
        this->m_ip = ip;
        this->m_container = container;
        this->callback = cb;

        m_packet.init(ptCommand,command,blocks,blocks_length);
    }

    virtual ~adbk_command() {
        xlog("~adbk_command");
        event_del(&socket_ev);
        event_del(&timeout_ev);
        close(fd);
    }

    static void timeout_callback(int fd,short event,void* arg) {
        xlog2("adbk_command timeout_callback");
        delete (adbk_command*)arg;
    }

    void set_timeout() {
        xlog("adbk_command set_timeout");

        timeout = ADBK_TIMEOUT;

        evtimer_set(&timeout_ev,timeout_callback,this);
        evtimer_add(&timeout_ev,&timeout);
    }

    void next_stage(void (*cb)(int,short,void*),short event) {
        event_set(&socket_ev,fd,event,cb,this);
        event_add(&socket_ev,NULL);
    }

};

static void adbk_recv_data_stage(int fd, short event,void *arg)
{
    xlog("adbk_recv_data_stage");
    adbk_command* c = (adbk_command*)arg;

    int ret = c->m_packet.recv_data(fd);
    if(ret == 1) {
        c->next_stage(adbk_recv_data_stage,EV_READ);
    } else {
        !ret ? c->callback(c) : xlog2("adbk_recv_stage: recv_data failed");
        delete c;
    }
}

static void adbk_recv_header_stage(int fd, short event,void *arg)
{
    xlog("adbk_recv_header_stage");
    adbk_command* c = (adbk_command*)arg;

    if( c->m_packet.recv_header(fd) < 0) {
        xlog2("adbk_recv_stage: recv_header failed");
        delete c;
    }

    c->next_stage(adbk_recv_data_stage,EV_READ);
}

static void adbk_send_stage(int fd,short event,void *arg)
{
    xlog("adbk_send_stage");
    adbk_command* c = (adbk_command*)arg;

    if(c->m_packet.send(fd) < 0) {
        xlog("adbk_send_stage: failed");
        delete c;
        return;
    }

    c->next_stage(adbk_recv_header_stage,EV_READ);
}

static int send_adbk_command(adbk_command *c)
{
    c->fd = create_connection(c->m_ip,ADBK_PORT);
    if(c->fd < 0) return -1;

    c->set_timeout();
    c->next_stage(adbk_send_stage,EV_WRITE);

    return 0;
}

template<int command,void (*callback)(IADBKCommandResult *cmd)>
static void execute(device_data *device,DC *container) {
    SCommand cmd = { command,0,0,btNone,0 };
    adbk_command* c = new adbk_command(device->addr,container,&cmd,callback);
    send_adbk_command(c);
}

template<void (*callback)(IADBKCommandResult* cmd),typename DataType>
static void execute(device_data* device,DC *container,SCommand *cmd,DataType *data=(char*)0) {
    size_t data_length = data ? sizeof(DataType) : 0;
    adbk_command *c = new adbk_command(device->addr,container,cmd,callback,data,data_length);
    send_adbk_command(c);
}

class ADBKService : public IDeviceProcessor
{
    DC *container;
public:
    ADBKService(DC* con):container(con) {}

    static void handle_sync_time(IADBKCommandResult* ret) {
        xlog("handle_sync_time");

        const SPacket *packet = ret->packet();
        struct __attribute__ ((packed)) sync_time_answer {
            SAnswer s;
        } *answer = (sync_time_answer*)packet->data;

        if(answer->s.err_code != answOK) {
            return xlog2("answer[%i] != answOk",answer->s.err_code);
        }
        if(packet->datalen < sizeof(*answer)) {
            return xlog2("answer[%i] < required[%i]",packet->datalen,sizeof(*answer));
        }

        if(device_data* device = ret->device()) {
            device->inner_time = device->last_answer_time;
        } else {
            xlog2("handle_sync_time: there is no device[%X] related to this command",ret->ip());
        }
    }

    static void handle_get_state(IADBKCommandResult* ret) {
        xlog("handle_get_state");

        const SPacket *packet = ret->packet();
        struct __attribute__ ((packed)) get_state_answer {
            SAnswer s;
            SStateBlock state;
        } *answer = (get_state_answer*)packet->data;

        if(answer->s.err_code != answOK) {
            return xlog2("answer[%i] != answOk",answer->s.err_code);
        }
        if(packet->datalen < sizeof(*answer)) {
            return xlog2("answer[%i] < required[%i]",packet->datalen,sizeof(*answer));
        }

        if(device_data* device = ret->device()) {
           snprintf(device->version,sizeof(device->version),"%s.%s",answer->state.sw_ver,
                    answer->state.card.hw_ver+strlen(answer->state.card.hw_ver)-2);

           uint64_t sn = 0;
           memcpy(&sn,answer->state.card.sn,sizeof(sn));

           ret->dc()->answer(device->addr,sn);
        } else {
            xlog2("handle_sync_time: there is no device[%X] related to this command",ret->ip());
        }
    }

    static void handle_get_events(IADBKCommandResult* ret) {
        xlog("handle_get_events");

        const SPacket *packet = ret->packet();
        struct __attribute__ ((packed)) get_events_answer {
            SAnswer s;
            FLASHEVENT event[0];
        } *answer = (get_events_answer*)packet->data;

        if(answer->s.err_code != answOK) {
            return xlog2("answer[%i] != answOk",answer->s.err_code);
        }

        if(answer->s.block_type ==  btNone) return; //there is no data from adbk

        if(answer->s.block_type != btEvent) {
            return xlog2("block[%i] != btEvent",answer->s.block_type);
        }
        xlog("answer->s.block_count[%i]",answer->s.block_count);
        const size_t required_length = sizeof(SAnswer) + sizeof(FLASHEVENT)*answer->s.block_count;
        if(packet->datalen <  required_length) {
            return xlog2("answer[%i] < required[%i]",packet->datalen,required_length);
        }

        if(device_data *device = ret->device()) {
            for(int i=0;i< answer->s.block_count;i++) {
                FLASHEVENT *f = answer->event + i;
                ret->dc()->eventStorage()->save_event(device,f);
                device->current_event_id = f->EventNumber;
            }
        } else {
             xlog2("handle_sync_time: there is no device[%X] related to this command",ret->ip());
        }
    }

    static void handle_get_last_event(IADBKCommandResult* ret) {
         xlog("handle_get_last_event");

         const SPacket *packet = ret->packet();
         struct __attribute__ ((packed)) get_last_event_answer {
             SAnswer s;
             FLASHEVENT event;
         } *answer = (get_last_event_answer*)packet->data;
         if(answer->s.err_code != answOK) {
             return xlog2("answer[%i] != answOk",answer->s.err_code);
         }
         if(packet->datalen < sizeof(*answer)) {
             return xlog2("answer[%i] < required[%i]",packet->datalen,sizeof(*answer));
         }

         if(device_data* device =  ret->device()) {
            device->last_event_id = answer->event.EventNumber;

            SCommand getEventsCommand = {
                cmdGetEvents,
                device->current_event_id + 1,
                EVENTS_TO_READ_COUNT,
                btNone,
                0
            };

            execute<handle_get_events>(device,ret->dc(),&getEventsCommand,(char*)0);
         } else {
             xlog2("handle_sync_time: there is no device[%X] related to this command",ret->ip());
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

void handle_get_update(IADBKCommandResult* ret)
{
     xlog("handle_get_update");

     const SPacket *packet = ret->packet();
     SAnswer *answer = (SAnswer*)packet->data;

     if(answer->err_code != answOK) {

         if(device_data* device = ret->device()) {
             device->progress = answer->err_code;
         }

         return xlog2("answer[%i] != answOk",answer->err_code);
     }

     if(packet->datalen < sizeof(*answer)) {
         return xlog2("answer[%i] < required[%i]",packet->datalen,sizeof(*answer));
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
