// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  unsigned long long o = 0;
  unsigned long long x = 0;
  int n = sscanf("17 1f", "%llo %llx", &o, &x);
  if (n != 2) return 1;
  if (o != 15) return 2;
  if (x != 31) return 3;
  return 0;
}
