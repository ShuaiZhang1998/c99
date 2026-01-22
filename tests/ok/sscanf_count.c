// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  int x = 0;
  int n = 0;
  int r = sscanf("123 abc", "%d %n", &x, &n);
  if (r != 1) return 1;
  if (x != 123) return 2;
  if (n != 4) return 3;
  return 0;
}
