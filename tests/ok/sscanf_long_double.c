// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  long li = 0;
  unsigned long lu = 0;
  double d = 0.0;
  int n = sscanf("-12345 65535 6.25", "%ld %lu %lf", &li, &lu, &d);
  if (n != 3) return 1;
  if (li != -12345) return 2;
  if (lu != 65535) return 3;
  if (d < 6.24 || d > 6.26) return 4;
  return 0;
}
