// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  int a = 0, b = 0, c = 0;
  int n = sscanf("10 010 0x10", "%i %i %i", &a, &b, &c);
  if (n != 3) return 1;
  if (a != 10) return 2;
  if (b != 8) return 3;
  if (c != 16) return 4;
  return 0;
}
