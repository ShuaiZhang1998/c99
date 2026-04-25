#ifndef C99CC_TIME_H
#define C99CC_TIME_H

#include <stddef.h>

typedef long long time_t;

struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};

time_t time(time_t* timer);
struct tm* localtime(const time_t* timer);
size_t strftime(char* s, size_t max, const char* format, const struct tm* tm);

#endif
