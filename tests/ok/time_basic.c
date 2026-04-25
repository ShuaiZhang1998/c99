// ARGS: -I include
// EXPECT: 0
#include <time.h>
#include <string.h>

int main() {
  time_t t = 0;
  time_t now = time(&t);
  if (now != t) return 1;

  // 2024-01-01 00:00:00 UTC
  t = 1704067200;
  struct tm* tm = localtime(&t);
  if (!tm) return 2;
  if (tm->tm_year != 124) return 3;
  if (tm->tm_mon != 0) return 4;
  if (tm->tm_mday != 1) return 5;
  if (tm->tm_hour != 0 || tm->tm_min != 0 || tm->tm_sec != 0) return 6;

  char buf[32];
  if (strftime(buf, sizeof(buf), "%F %T", tm) != 19) return 7;
  if (strcmp(buf, "2024-01-01 00:00:00") != 0) return 8;

  if (strftime(buf, 5, "%F", tm) != 0) return 9;
  if (strftime(buf, sizeof(buf), "%%Y=%Y", tm) != 7) return 10;
  if (strcmp(buf, "%Y=2024") != 0) return 11;

  return 0;
}
