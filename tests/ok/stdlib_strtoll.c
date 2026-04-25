// ARGS: -I include
// EXPECT: 0
#include <stdlib.h>

int main() {
  char* end = 0;

  long long a = strtoll("  -123xyz", &end, 10);
  if (a != -123) return 1;
  if (*end != 'x') return 2;

  long long b = strtoll("0x20", &end, 0);
  if (b != 32) return 3;

  unsigned long long c = strtoull("077", &end, 0);
  if (c != 63) return 4;

  unsigned long long d = strtoull("-1", &end, 10);
  if (d != (unsigned long long)-1) return 5;

  unsigned long long e = strtoull("FFFF", &end, 16);
  if (e != 65535ull) return 6;
  if (*end != '\0') return 7;

  return 0;
}
