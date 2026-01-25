// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  char buf[2];
  int n = sscanf("ab", "%2c", buf);
  if (n != 1) return 1;
  if (buf[0] != 'a') return 2;
  if (buf[1] != 'b') return 3;
  return 0;
}
