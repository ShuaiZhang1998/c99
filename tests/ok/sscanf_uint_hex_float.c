// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
int main() {
  unsigned int u = 0;
  unsigned int x = 0;
  float f = 0.0f;
  int n = sscanf(" 429 1f 3.5e1", "%u %x %f", &u, &x, &f);
  if (n != 3) return 1;
  if (u != 429) return 2;
  if (x != 31) return 3;
  if (f < 34.9f || f > 35.1f) return 4;
  return 0;
}
