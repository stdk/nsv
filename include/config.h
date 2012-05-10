#ifndef CONFIG_H
#define CONFIG_H

#define PLATFORM_TS7500
//#define PLATFORM_TS7260

//it's possible to combine console and syslog logging
//still you should be aware of internal limitations (daemon state, etc)
#define LOG_CONSOLE
//#define LOG_SYSLOG

//logging level
//#define LOG
#define LOG2

//only one type of can should be defined
#define CAN_TS7500
//#define CAN_LINCAN

#endif // CONFIG_H

