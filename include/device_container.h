#ifndef DEVICE_CONTAINER
#define DEVICE_CONTAINER

#include <can_device.h>
#include <config.h>

#include <stdint.h>
#include <memory>

typedef std::map<uint64_t,device_data> DeviceStorage;
typedef std::map<uint32_t,device_data*> ActiveDeviceStorage;

struct IDeviceProcessor
{
    virtual void process(device_data* device)=0;
    virtual ~IDeviceProcessor() {}
};

typedef struct _FLASHEVENT	// 66 bytes
{
  uint32_t EventNumber; 		// 4
  uint32_t Time;			// 4
  uint16_t HallID; 		// 2
  uint8_t HallDeviceID;   	// 1
  uint8_t HallDeviceType; 	// 1
  uint8_t EventCode;		// 1
  uint8_t ErrorCode;		// 1
  uint8_t DataLen;		// 1
  uint8_t EventData[51];		// 51
} __attribute__ ((packed)) FLASHEVENT;

class IEventStorage
{
public:
    virtual int save_event(device_data * device,FLASHEVENT* event)=0;
    virtual int flush()=0;
    virtual uint32_t event_id()=0;
    virtual ~IEventStorage() {}
};

#define CAN_NORMAL 1
#define CAN_BUSY   2

class DC
{
        uint32_t can_mode;

        std::auto_ptr< IEventStorage > event_storage;

	DeviceStorage storage;
	ActiveDeviceStorage active;
private:
        DC(const DC&);
        DC& operator=(const DC&);

public:
        DC();
        ~DC();

        int state();

        IEventStorage* eventStorage();

        //helper functions that return number of devices currently active
        uint32_t activeCount() const;
        uint32_t storageCount() const;

        device_data* deviceBySN(uint64_t sn);
        device_data* deviceByAddr(uint32_t addr);

        void answer(uint32_t addr,uint64_t sn);

        //There are two possible ways to call an action for all active devices.
        //We can either check device status before performing an action
        //and change storage contents according to result or assume
        //that this check is redundant and skip it. Consequently, the first way
        //allows us to update an actual list of active devices and perform
        //given action only for them, while second way allows us either to skip
        //redundant check or use somewhat outdated information for our objectives
        //(e.g. when time changes we can't be sure our devices will remain active
        //in program POV)
        void for_each_active(int type,IDeviceProcessor * processor,bool check=false);
        void for_each(int type,IDeviceProcessor * processor);

        void deactivateByAddr(uint32_t addr);
        int removeBySN(uint64_t sn);

        uint32_t get_can_mode();
        void set_can_mode(uint32_t mode);
};

#endif //DEVICE_CONTAINER
