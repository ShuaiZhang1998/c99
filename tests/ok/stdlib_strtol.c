// ARGS: -I include
// EXPECT: 0
#include <stdlib.h>

int main() {
  char* end = 0;
  long v1 = strtol("123", &end, 10);
  if (v1 != 123) return 1;
  if (*end != '\0') return 2;

  long v2 = strtol("0x10", &end, 0);
  if (v2 != 16) return 3;

  long v3 = strtol("077", &end, 0);
  if (v3 != 63) return 4;

  unsigned long v4 = strtoul("FF", &end, 16);
  if (v4 != 255) return 5;
  return 0;
}
