// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  int x = 0;
  int n = sscanf("12 34", "%*d %d", &x);
  if (n != 1) return 1;
  if (x != 34) return 2;
  return 0;
}
