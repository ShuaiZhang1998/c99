#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
struct timeval {
  long tv_sec;
  long tv_usec;
};
extern int gettimeofday(struct timeval* tv, void* tz);
#endif

static int is_leap_year(int year) {
  if (year % 400 == 0) return 1;
  if (year % 100 == 0) return 0;
  return (year % 4 == 0) ? 1 : 0;
}

static int days_in_year(int year) {
  return is_leap_year(year) ? 366 : 365;
}

static int days_in_month(int year, int month) {
  static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 1 && is_leap_year(year)) return 29;
  return days[month];
}

static void write_two_digits(char* out, int v) {
  out[0] = (char)('0' + (v / 10) % 10);
  out[1] = (char)('0' + (v % 10));
}

static void write_four_digits(char* out, int v) {
  out[0] = (char)('0' + (v / 1000) % 10);
  out[1] = (char)('0' + (v / 100) % 10);
  out[2] = (char)('0' + (v / 10) % 10);
  out[3] = (char)('0' + (v % 10));
}

static struct tm* breakdown_time(time_t t) {
  static struct tm out;
  long long days = t / 86400;
  long long rem = t % 86400;
  if (rem < 0) {
    rem += 86400;
    --days;
  }

  out.tm_hour = (int)(rem / 3600);
  rem %= 3600;
  out.tm_min = (int)(rem / 60);
  out.tm_sec = (int)(rem % 60);

  int year = 1970;
  while (days >= days_in_year(year)) {
    days -= days_in_year(year);
    ++year;
  }
  while (days < 0) {
    --year;
    days += days_in_year(year);
  }

  out.tm_year = year - 1900;
  out.tm_yday = (int)days;

  int month = 0;
  while (days >= days_in_month(year, month)) {
    days -= days_in_month(year, month);
    ++month;
  }
  out.tm_mon = month;
  out.tm_mday = (int)days + 1;
  out.tm_wday = (int)((4 + (t / 86400)) % 7);
  if (out.tm_wday < 0) out.tm_wday += 7;
  out.tm_isdst = 0;
  return &out;
}

time_t time(time_t* timer) {
  time_t now = 0;
#ifdef _WIN32
  FILETIME ft;
  ULARGE_INTEGER u;
  GetSystemTimeAsFileTime(&ft);
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  now = (time_t)((u.QuadPart - 116444736000000000ULL) / 10000000ULL);
#else
  struct timeval tv;
  if (gettimeofday(&tv, 0) == 0) {
    now = (time_t)tv.tv_sec;
  }
#endif
  if (timer) *timer = now;
  return now;
}

struct tm* localtime(const time_t* timer) {
  if (!timer) return 0;
  return breakdown_time(*timer);
}

size_t strftime(char* s, size_t max, const char* format, const struct tm* tm) {
  if (!s || !format || !tm || max == 0) return 0;
  size_t out = 0;
  for (size_t i = 0; format[i]; ++i) {
    char c = format[i];
    char tmp[5];
    const char* src = 0;
    size_t n = 0;
    if (c != '%') {
      tmp[0] = c;
      src = tmp;
      n = 1;
    } else {
      ++i;
      c = format[i];
      if (!c) return 0;
      switch (c) {
        case '%':
          tmp[0] = '%';
          src = tmp;
          n = 1;
          break;
        case 'Y':
          write_four_digits(tmp, tm->tm_year + 1900);
          src = tmp;
          n = 4;
          break;
        case 'm':
          write_two_digits(tmp, tm->tm_mon + 1);
          src = tmp;
          n = 2;
          break;
        case 'd':
          write_two_digits(tmp, tm->tm_mday);
          src = tmp;
          n = 2;
          break;
        case 'H':
          write_two_digits(tmp, tm->tm_hour);
          src = tmp;
          n = 2;
          break;
        case 'M':
          write_two_digits(tmp, tm->tm_min);
          src = tmp;
          n = 2;
          break;
        case 'S':
          write_two_digits(tmp, tm->tm_sec);
          src = tmp;
          n = 2;
          break;
        case 'F':
          write_four_digits(tmp, tm->tm_year + 1900);
          if (out + 10 >= max) return 0;
          for (size_t j = 0; j < 4; ++j) s[out++] = tmp[j];
          s[out++] = '-';
          write_two_digits(tmp, tm->tm_mon + 1);
          s[out++] = tmp[0];
          s[out++] = tmp[1];
          s[out++] = '-';
          write_two_digits(tmp, tm->tm_mday);
          s[out++] = tmp[0];
          s[out++] = tmp[1];
          continue;
        case 'T':
          if (out + 8 >= max) return 0;
          write_two_digits(tmp, tm->tm_hour);
          s[out++] = tmp[0];
          s[out++] = tmp[1];
          s[out++] = ':';
          write_two_digits(tmp, tm->tm_min);
          s[out++] = tmp[0];
          s[out++] = tmp[1];
          s[out++] = ':';
          write_two_digits(tmp, tm->tm_sec);
          s[out++] = tmp[0];
          s[out++] = tmp[1];
          continue;
        default:
          tmp[0] = '%';
          tmp[1] = c;
          src = tmp;
          n = 2;
          break;
      }
    }
    if (out + n >= max) return 0;
    for (size_t j = 0; j < n; ++j) s[out++] = src[j];
  }
  s[out] = '\0';
  return out;
}
