#include "log.h"

#include <stdarg.h>

#ifdef LOG_SYSLOG
#include <syslog.h>
#endif

#ifdef LOG_CONSOLE
#include <time.h>
#include <stdio.h>
#endif

void log_f(const char *format, ...)
{
  va_list argptr;
  va_start(argptr, format);

#ifdef LOG_CONSOLE
  time_t ltime;
  time( &ltime );
  struct tm *now;
  now = localtime( &ltime );

  fprintf(stderr,"\n%4u-%02u-%02u %02u:%02u:%02u ",
		  now->tm_year+1900, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);

  vfprintf(stderr,format, argptr);
#endif  
    
#ifdef LOG_SYSLOG  
  vsyslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), format, argptr);
#endif  

  va_end(argptr);
}
