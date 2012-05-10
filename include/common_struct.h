#ifndef COMMON_STRUCT_H
#define COMMON_STRUCT_H

#include <stdint.h>
#include <time.h>

struct __attribute__ ((packed)) DATE
{
  uint16_t year :5;
  uint16_t month :4;
  uint16_t day :5;
  uint16_t empty :2;
};

struct __attribute__ ((packed)) TIME
{
  uint16_t hour :5;
  uint16_t minute :6;
  uint16_t second :5;
};

struct  __attribute__ ((packed)) CLOCKDATA	// 4 bytes
{
  TIME time;
  DATE date;

  CLOCKDATA() {

  }

  CLOCKDATA& operator=(const struct tm now) {
        this->time.hour 	= now.tm_hour;
        this->time.minute 	= now.tm_min;
        this->time.second 	= now.tm_sec/2;
        this->date.year 	= now.tm_year-100;
        this->date.month 	= now.tm_mon+1;
        this->date.day		= now.tm_mday;
        this->date.empty	= 0;
        return *this;
  }

  static CLOCKDATA now() {
      time_t ltime;
      ::time( &ltime );
      struct tm *now = localtime( &ltime );
      CLOCKDATA cd;
      cd = *now;
      return cd;
  }

  CLOCKDATA(uint32_t udt) {
    memcpy(this, &udt, sizeof(udt));
  }

};

#endif // COMMON_STRUCT_H
