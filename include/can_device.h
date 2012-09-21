#ifndef DEVICE_H
#define DEVICE_H

#include <time.h>
#include <stdint.h>
#include <vector>
#include <map>

#include "log.h"

#define TYPE_ANY   0xFF
#define TYPE_CAN   0x01
#define TYPE_ADBK  0x02

struct device_data
{
    void answer(uint32_t addr,uint64_t sn);
    void error(uint32_t event_id);
    int active() const;
    int new_last_event_id(uint32_t new_id);

    inline uint16_t get_addr() const {
        if(get_type() == TYPE_ADBK) {
            return 0x200 + ((char*)&addr)[3];
        } else {
            return addr;
        }
    }

    inline int get_type() const {
        return ((char*)&addr)[3] ? TYPE_ADBK : TYPE_CAN;
    }

    uint64_t sn;
    uint32_t addr;
    uint32_t errors;
    time_t last_answer_time;
    time_t inner_time;
    uint32_t answered;
    uint32_t last_event_id;
    uint32_t current_event_id;
    int stoplist;
    int progress;
    char version[20];
};

struct can_device_data : public device_data
{
    static void get_time(char * data/*[CAN_MESSAGE_LENGTH]*/,struct tm * t);
    void set_time(char * data/*[CAN_MESSAGE_LENGTH]*/);
};

struct adbk_device_data : public device_data
{

};

#endif //DEVICE_H
