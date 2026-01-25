// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  long long a = 0;
  unsigned long long b = 0;
  long long c = 0;
  int n = sscanf("-9 255 077", "%lld %llu %lli", &a, &b, &c);
  if (n != 3) return 1;
  if (a != -9) return 2;
  if (b != 255) return 3;
  if (c != 63) return 4;
  return 0;
}
