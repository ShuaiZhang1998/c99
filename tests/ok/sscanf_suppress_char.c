// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  char c = 0;
  int n = sscanf("abc", "%*2c%c", &c);
  if (n != 1) return 1;
  if (c != 'c') return 2;
  return 0;
}
