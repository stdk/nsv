#ifndef LOG_H
#define LOG_H

#include "config.h"

/* thread-safe */
void log_f(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

#ifndef LOG
#define xlog(...)
#else
#define xlog log_f
#endif

#ifndef LOG2
#define xlog2(...)
#else
#define xlog2 log_f
#endif

#endif //LOG_H
