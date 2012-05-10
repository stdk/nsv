#include "storage.h"
#include "log.h"
#include <common_struct.h>

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sqlite3.h>

#include <stdarg.h>

//#define FORCED_SYNC

#define EVENTS_OPEN_FLAGS (O_RDWR /* | O_SYNC*/)

#define ENV_DEVICES          "DEVICES"
#define ENV_DEVICES_NEW      "DEVICES_NEW"
#define ENV_DEVICES_SAVE     "DEVICES_SAVE"
#define ENV_STORAGE          "STORAGE"
#define EVENTS_NUMBER       500000
#define EVENT_SIZE          sizeof(EVENT)

#define ENV_DB             "DB"

#define ENV_STORAGE_CASH "STORAGE_CASH"
#define CASH_EVENTS_NUMBER 1000

typedef struct _EVENT // 84
{
  uint32_t main_id;	// 4
  uint16_t addr;		// 2
  uint8_t  sn[8];		// 8
  uint32_t  regTime; 	// 4
  FLASHEVENT event;	// 66
  _EVENT()
  {
    main_id 	=
    addr 	=
    regTime 	= 0;
    memset(sn, 0, sizeof(sn));
  }
} __attribute__ ((packed)) EVENT;

struct storage_state
{
    uint32_t id;
    off_t pos;
    uint32_t pos_limit;
};

static int get_event_storage_state(storage_state * state,const char* storage,size_t events_number,size_t event_size);

class FileSaver : public IDeviceProcessor
{
	FILE* dev_file;
public:
        FileSaver(FILE* _dev_file):dev_file(_dev_file) {}

        void process(device_data* device) {
                fprintf(dev_file,"%X %llX ",device->addr,device->sn);
		fprintf(dev_file,"%u\n",device->current_event_id);
	}
};

class SQLiteDatabase
{
   sqlite3 *db;
   bool opened;
   bool ok;
public:
   SQLiteDatabase(const char* db_filename) {
       xlog2("sqlite3_open[%s]",db_filename);
       opened = sqlite3_open(db_filename, &db) == SQLITE_OK;
       if(!opened) {
           xlog2("sqlite3_open: %s", sqlite3_errmsg(db));
           return;
       }
   }
   ~SQLiteDatabase() {
       if(opened) {
           xlog2("sqlite3_close");
           sqlite3_close(db);
       }
   }

    bool executeNonQuery(const char* command) {
        xlog("executeNonQuery[%s]",command);
        char *errmsg = 0;
        ok = sqlite3_exec(db,command,0,0,&errmsg) ==  SQLITE_OK;
        if(!ok) {
            xlog2("sqlite3_exec[%s]: %s",command,errmsg);
            sqlite3_free(errmsg);
        }
        return ok;
    }

    bool valid() {
        return ok;
    }

    operator sqlite3*() {
        return db;
    }

    const char* errorMessage() {
        return sqlite3_errmsg(db);
    }
};

class SQLitePreparedStatement
{
    SQLiteDatabase &db;
    sqlite3_stmt *stmt;
    bool ok;
public:
    SQLitePreparedStatement(SQLiteDatabase &_db,const char* command):db(_db) {
        xlog("create statement[%s]",command);
        ok = sqlite3_prepare_v2(db,command,strlen(command),&stmt,0) == SQLITE_OK;
        if(!ok) {
            xlog2("sqlite3_prepare_v2: %s",sqlite3_errmsg(db));
            return;
        }
    }
    ~SQLitePreparedStatement() {
        xlog("finalize statement");
        ok = sqlite3_finalize(stmt) == SQLITE_OK;
        if(!ok) {
            xlog2("sqlite3_finalize: %s", sqlite3_errmsg(db));
            return;
        }
    }
    bool reset() {
        xlog("statement reset");
        ok = sqlite3_reset(stmt) == SQLITE_OK;
        if(!ok) {
            xlog2("sqlite3_reset: %s", db.errorMessage());
        }
        return ok;
    }

    bool step() {
        xlog("statement step");
        ok = sqlite3_step(stmt) == SQLITE_DONE;
        if(!ok) {
           xlog2("sqlite3_step: %s", db.errorMessage());
        }
        return ok;
    }

    void bind_text(int column,const char *format, ...) __attribute__ ((format (printf, 3, 4)));
    void bind_int(int column,int value);


    bool valid() {
        return ok;
    }
};

void SQLitePreparedStatement::bind_text(int column,const char *format, ...) {
     va_list argptr;
     va_start(argptr, format);
     sqlite3_bind_text(stmt,column,sqlite3_vmprintf(format,argptr),-1,sqlite3_free);
     va_end(argptr);
}

void SQLitePreparedStatement::bind_int(int column,int value) {
    sqlite3_bind_int(stmt,column,value);
}

class SQLiteTransaction
{
    SQLiteDatabase *db;
    bool ok;
public:
    SQLiteTransaction(SQLiteDatabase *_db):db(_db) {
        ok = db->executeNonQuery("begin transaction");
    }
    ~SQLiteTransaction() {
        ok = db->executeNonQuery("commit transaction");
    }
    bool valid() {
        return ok;
    }
};

class SQLiteSaver : public IDeviceProcessor
{
    SQLiteDatabase db;
    SQLiteTransaction transaction;
    SQLitePreparedStatement statement;
    bool ok;
public:
    SQLiteSaver(const char* db_filename):
        db(db_filename),
        transaction(&db),
        statement(db,"insert or replace into device values(?,?,?)")
    {
        ok = db.valid() && transaction.valid() && statement.valid();
    }

    virtual ~SQLiteSaver() {

    }

    bool valid() {
        return ok;
    }

    bool removeExistingDevices() {
        ok = db.executeNonQuery("delete from device");
        return ok;
    }

    void process(device_data* device) {
        xlog("SQLiteSaver process [%X %llX]",device->addr,device->sn);

        statement.reset();
        statement.bind_text(1,"%X",device->addr);
        statement.bind_text(2,"%llX",device->sn);
        statement.bind_int(3,device->current_event_id);
        statement.step();
    }

};

int save_devices(DC& container)
{
    xlog2("save_devices[%i]",container.storageCount());

    container.eventStorage()->flush();

    SQLiteSaver saver(getenv(ENV_DB));
    if(saver.valid()) {
        saver.removeExistingDevices();
        container.for_each(TYPE_ANY,&saver);
    }

    return 0;
}

int save_devices_old(DC& container)
{
        xlog2("save_devices[%i]",container.storageCount());

        container.eventStorage()->flush();
	
        char* new_device_storage = getenv(ENV_DEVICES_NEW);
        char* save_cmd = getenv(ENV_DEVICES_SAVE);

        FILE* dev_file = fopen(new_device_storage,"w");
	if(!dev_file) {
                xlog2("cannot open file[%s] in save_devices",new_device_storage);
		return -1;
	}

        FileSaver saver(dev_file);
        container.for_each(TYPE_ANY,&saver);
	
        #ifdef FORCED_SYNC
            if(-1 == fsync(fileno(dev_file))) {
                xlog2("fsync: %s",strerror(errno));
            }
        #endif

	if(fclose(dev_file)) return -1;
	
        if(system(save_cmd) != 0) {
                xlog2("Error during backup");
		return -1;
	} else {
		xlog("backup ok");
	}

	return 0;
}

class SQLiteLoader
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    bool ok;

public:
    SQLiteLoader(const char* db_filename) {
        ok = sqlite3_open(db_filename, &db) == SQLITE_OK;
        if(!ok) {
            xlog2("sqlite3_open: %s", sqlite3_errmsg(db));
            return;
        }

        const char *query = "select addr,sn,id from device";
        ok = sqlite3_prepare_v2(db,query,strlen(query),&stmt,0) == SQLITE_OK;
        if(!ok) {
            xlog2("sqlite3_prepare_v2: %s",sqlite3_errmsg(db));
            return;
        }
    }

    ~SQLiteLoader() {
        ok = sqlite3_finalize(stmt) == SQLITE_OK;
        if(!ok) {
            xlog2("sqlite3_finalize: %s", sqlite3_errmsg(db));
        }
        xlog2("sqlite3_close");
        sqlite3_close(db);
    }

    bool valid() {
        return ok;
    }

    int load(IDeviceContainer* adder) {
        xlog2("SQLiteLoader::load adder[%p]",adder);

        ok = sqlite3_reset(stmt) == SQLITE_OK;
        if(!ok) {
            xlog2("sqlite3_reset: %s", sqlite3_errmsg(db));
            return -1;
        }

        while(sqlite3_step(stmt) == SQLITE_ROW) {
            uint32_t addr;
            uint64_t sn;
            uint32_t event_id;

            sscanf((char*)sqlite3_column_text(stmt,0),"%X",&addr);
            sscanf((char*)sqlite3_column_text(stmt,1),"%llX",&sn);
            event_id = sqlite3_column_int(stmt,2);

            adder->add_device(addr,sn,event_id);
        }
        return 0;
    }
};

int load_devices(IDeviceContainer* adder)
{
    xlog2("load_devices");

    SQLiteLoader loader(getenv(ENV_DB));
    if(loader.valid()) loader.load(adder);

    return 0;
}

int load_devices_old(IDeviceContainer* adder)
{
        xlog("load_devices adder");

        char* device_storage = getenv(ENV_DEVICES);
        FILE* dev_file = fopen(device_storage,"r");
	if(!dev_file) {
                xlog2("cannot open devices file[%s] to read",device_storage);
		return -1;
	}

	while(!feof(dev_file)) {
		uint64_t sn;
                uint32_t addr;
		uint32_t event_id;

                fscanf(dev_file,"%X %llX %u\n",&addr,&sn,&event_id);

                int add = sn && addr;
                if(add) {
                    adder->add_device(addr,sn,event_id);
                }

                xlog2("device[%X][%llX][%u] verdict[%i]",addr,sn,event_id,add);
	}

        fclose(dev_file);

	return 0;
}

static int prepare_event(device_data * device,FLASHEVENT * flashevent,EVENT* ev)
{
    xlog("prepare_event");

    if(0 == flashevent->EventCode) {
            xlog2("Incorrect event code: number[%i] addr[%hhX:%hhX]",flashevent->EventNumber,
                                                                     flashevent->HallDeviceType,
                                                                     flashevent->HallDeviceID);
            return -1;
    }

    time_t ltime;
    time( &ltime );
    struct tm *now = localtime( &ltime );
    CLOCKDATA cd;
    cd = *now;

    ev->addr = device->get_addr();
    ev->regTime = *(uint32_t*)&cd;
    memcpy(ev->sn,&device->sn,sizeof(device->sn));
    memcpy(&ev->event,flashevent,sizeof(*flashevent));

    return 0;
}

class BufferedEventStorage :  public IEventStorage
{
    storage_state state;
    storage_state state_cash;

    EVENT * buffer;
    const uint32_t buf_size;
    const EVENT * buffer_end;
    EVENT * buffer_ptr;
public:
    static BufferedEventStorage* create(storage_state current_state,storage_state current_state_cash) {
        xlog2("BufferedEventStorage::create");

        const char* ENV_SIZE = "EVENT_STORAGE_SIZE";
        uint32_t size = 30;
        const char* str_size = getenv(ENV_SIZE);
        if(str_size) {
            size = atoi(str_size);
        }

        xlog2("BufferedStorage size[%i]",size);

        EVENT* buffer = new EVENT[size];
        if(!buffer) {
            xlog2("buffer init: failed");
            return 0;
        }

        return new BufferedEventStorage(current_state,current_state_cash,size,buffer);
    }

    BufferedEventStorage(storage_state current_state,storage_state current_state_cash,uint32_t _size,EVENT * _buffer)
        :state(current_state),state_cash(current_state_cash),buffer(_buffer),buf_size(_size)
    {
        buffer_end = _buffer + buf_size;
        buffer_ptr = _buffer;
    }

    virtual ~BufferedEventStorage() {
        xlog2("~BufferedEventStorage");
        delete[] buffer;
    }

    virtual uint32_t event_id()
    {
        return state.id;
    }

    int save_to_storage(int fd,storage_state &state,EVENT *e) {
        lseek(fd,state.pos,SEEK_SET);
        ssize_t bytes_write = write(fd,e,sizeof(*e));
        if(-1 == bytes_write) {
            xlog2("event write: %s",strerror(errno));
            return -1;
        }
        if(bytes_write != sizeof(*e)) {
            xlog2("not enough bytes written[%i]",bytes_write);
                return -1;
        }
        //on success we can increase current state parameters
        state.id += 1;
        state.pos = (state.pos + sizeof(*e)) % state.pos_limit;
    }

    virtual int flush() {
        xlog2("BufferedEventStorage flush[%i]",buffer_ptr - buffer);

        const char* storage_filename = getenv(ENV_STORAGE);
        const char* cash_storage_filename = getenv(ENV_STORAGE_CASH);

        int fd = open(storage_filename,O_WRONLY);
        if(fd == -1) {
            xlog2("storage[%s] open: %s",storage_filename,strerror(errno));
            return -1;
        }

        int fd_cash = open(cash_storage_filename,O_WRONLY);
        if(fd_cash == -1) {
            xlog2("cash_storage[%s] open: %s",cash_storage_filename,strerror(errno));
            return -2;
        }

        EVENT* flush_buffer_ptr = buffer;
        while(flush_buffer_ptr != buffer_ptr) {
            flush_buffer_ptr->main_id = state.id;

            save_to_storage(fd,state,flush_buffer_ptr);

            switch(flush_buffer_ptr->event.HallDeviceType) {
            case 2:
            case 5:
                save_to_storage(fd_cash,state_cash,flush_buffer_ptr);
                break;
            }

            flush_buffer_ptr++;
        }
        close(fd);
        close(fd_cash);

        buffer_ptr = buffer;

        xlog2("cash_state id[%i]",state_cash.id);

        return 0;
    }

    virtual int save_event(device_data* device,FLASHEVENT* flashevent) {
        xlog("save_event number[%i] addr[%hhX:%hhX]",flashevent->EventNumber,
                                                      flashevent->HallDeviceType,
                                                      flashevent->HallDeviceID);

        int ret = prepare_event(device,flashevent,buffer_ptr);
        if(ret) return ret;
        ++buffer_ptr;

        xlog("diff[%i]",buffer_ptr - buffer);
        if(buffer_ptr == buffer_end) {
         flush();
        }

        return 0;
    }

};

IEventStorage* createEventStorage()
{
    const char* storage_filename = getenv(ENV_STORAGE);

    storage_state current_state = {0};
    int ret = get_event_storage_state(&current_state,storage_filename,EVENTS_NUMBER,sizeof(EVENT));
    if(ret == -1) {
        xlog2("createEventStorage: failed on current_state");
        return 0;
    }
    xlog2("state id[%i]",current_state.id);

    const char* cash_storage_filename = getenv(ENV_STORAGE_CASH);
    storage_state current_state_cash = {0};
    ret = get_event_storage_state(&current_state_cash,cash_storage_filename,CASH_EVENTS_NUMBER,sizeof(EVENT));
    if(ret == -1) {
        xlog2("createEventStorage: failed on current_state_cash");
        return 0;
    }
    xlog2("cash_state id[%i]",current_state_cash.id);

    return BufferedEventStorage::create(current_state,current_state_cash);
}

/*static int check_event_file_consistency(int fd)
{
    xlog2("check_event_file_consistency");

    const size_t length = EVENTS_NUMBER * sizeof(EVENT);

    EVENT *events = (EVENT*)mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (events == MAP_FAILED) {
        xlog2("mmap failed: %s",strerror(errno));
        return -1;
    }

    const EVENT* end = events + length;
    EVENT* event = events + 1;
    while(++event != end) {
        uint32_t diff = event->main_id - (event-1)->main_id;
        ptrdiff_t pos = event - events;
        switch(diff) {
            case 0:  xlog2("zero diff val[%i] at pos[%i]",event->main_id,pos);
                     break;
            case 1:  xlog2("stable[%i] at pos[%i]",event->main_id,pos);
                     break;
            default: xlog2("unusual diff[%i] at pos[%i] values[][]",diff,pos);
                     break;
        }
    };

    if (munmap(events, length) == -1) {
        xlog2("munmap failed: %s",strerror(errno));
        return -2;
    }

    return 0;
}*/

static int create_event_file(const char* filename,const size_t events_number,size_t event_size)
{
    xlog2("create_event_file");
    int event_file_fd = creat(filename,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);// -rw-r--r--
    if(-1 == event_file_fd) {
        xlog2("creat: %s",strerror(errno));
        return -1;
    } else {
        const size_t blocks = 200;
        const ssize_t block_size = events_number/blocks*event_size;
        void * block_buf = calloc(block_size,1);
        for(size_t i=0;i<blocks;i++) {
            if(write(event_file_fd,block_buf,block_size) != block_size) {
                xlog2("write: %s",strerror(errno));
                return -2;
            }
        }
        free(block_buf);

        if(-1 == close(event_file_fd)) {
            xlog2("close: %s",strerror(errno));
            return -3;
        }

        if(-1 == (event_file_fd = open(filename,EVENTS_OPEN_FLAGS))) {
            xlog2("open: %s",strerror(errno));
            return -5;
        } else {
            return event_file_fd;
        }

        return 0;
    }
}

static storage_state scan_storage(int fd_ev,const size_t events_number,size_t event_size)
{
  storage_state result = {0,0,event_size*events_number};

  EVENT ev;
  uint32_t ids=0;
  uint32_t idt=0;

  off_t bgnPoint = 0;
  off_t endPoint = events_number * event_size;
  off_t CmpPointLen;

  if(lseek(fd_ev, 0, SEEK_SET) == -1) {
	  xlog2("lseek: %s",strerror(errno));
  }
  if(read(fd_ev, &ev, event_size) == -1) {
	  xlog2("read: %s",strerror(errno));
  }
  ids = ev.main_id;
  xlog("ids[%i]",ids);

  while(1)
  {
    CmpPointLen = (((endPoint-bgnPoint)/event_size)/2)*event_size;
	xlog("cmp[%i]",CmpPointLen);
    if((size_t)CmpPointLen<event_size)
    {
      if(lseek(fd_ev, bgnPoint, SEEK_SET)==-1) {
		  xlog2("lseek: %s",strerror(errno));
	  }
      if(read(fd_ev, &ev, event_size)==-1) {
		  xlog2("read: %s",strerror(errno));
	  }
      result.id = ev.main_id;
      result.pos = bgnPoint;
	  
      return result;
    }
	if(lseek(fd_ev, bgnPoint+CmpPointLen, SEEK_SET)==-1) {
		xlog2("lseek: %s",strerror(errno));
	}
	if(read(fd_ev, &ev, event_size)==-1) {
		xlog2("read: %s",strerror(errno));
	}

    idt = ids + ((bgnPoint - 0 + CmpPointLen)/event_size);
	

    if (ev.main_id == idt)
      bgnPoint += CmpPointLen;
    else
      endPoint = CmpPointLen + bgnPoint;
	xlog("b[%i] e[%i]",bgnPoint,endPoint);
  }
}

// returns opened file descriptor to a correctly formed file
// (and attempts to form it when there is no given file)
// returns values:
// -1 : creating new event file failed
// -2 : zero filling new event file failed
// -3 : close on new event file failed
// -4 : stat failed with errno != ENOENT that means we cannot create new event file
// -5 : opening event file failed
// -6 : incorrect event file length
static int init_event_file(const char* filename,const size_t events_number,size_t event_size)
{
    //we are using stat here cause it can follow symlinks -
    //so it's possible to place event file somewhere else physically
    struct stat event_file_stat;
    if(-1 == stat(filename,&event_file_stat)) {
        xlog2("stat: %s",strerror(errno));
        if(errno == ENOENT) { // when there is no file it means we should try to create it
            return create_event_file(filename,events_number,event_size);
        } else {
            return -4;
        }
    }

    const off_t required_size = (off_t)events_number * (off_t)event_size;
    if(event_file_stat.st_size < required_size)
    {
        return create_event_file(filename,events_number,event_size);
    }

    int event_file_fd = open(filename,EVENTS_OPEN_FLAGS);
    if(-1 == event_file_fd) {
        xlog2("open[%s]: %s",filename,strerror(errno));
        return -5;
    }

    return event_file_fd;
}

static int get_event_storage_state(storage_state *state,const char* storage,size_t events_number,size_t event_size)
{
        int event_file_fd = init_event_file(storage,events_number,event_size);
        if(event_file_fd < 0) {
            xlog2("init_event_file[%i]",event_file_fd);
            return -1;
        }
	
        xlog2("storage open: success");
	
        *state = scan_storage(event_file_fd,events_number,event_size);
        close(event_file_fd);

        //prepare state for adding new events
        if(state->id != 0) state->pos += event_size;
        state->id += 1;
	
        xlog2("storage state: pos[%lu] id[%u]",state->pos,state->id);

        return 0;
}

char* get_file_contents(const char* name,int *size)
{
        FILE* file = fopen(name, "rb");
        if (file == NULL) return 0;
        fseek(file, 0, SEEK_END);
        *size = ftell(file);
        rewind(file);
        char* chars = (char*)malloc(*size + 1);
        chars[*size] = '\0';
        for (int i = 0; i < *size;) {
                int read = fread(&chars[i], 1, *size - i, file);
                i += read;
        }
        fclose(file);
        return chars;
}
