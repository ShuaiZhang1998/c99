// ARGS: -I include
// EXPECT: 0
#include <string.h>
#include <errno.h>

int main() {
  if (strcmp(strerror(0), "no error") != 0) return 1;
  if (strcmp(strerror(ENOENT), "no such file or directory") != 0) return 2;
  if (strcmp(strerror(EINVAL), "invalid argument") != 0) return 3;
  if (strcmp(strerror(EIO), "input/output error") != 0) return 4;
  if (strcmp(strerror(ENOMEM), "not enough memory") != 0) return 5;
  if (strcmp(strerror(EACCES), "permission denied") != 0) return 6;
  if (strcmp(strerror(999), "unknown error") != 0) return 7;
  return 0;
}
