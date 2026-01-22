// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  unsigned int u = 0;
  int n = sscanf("17", "%o", &u);
  if (n != 1) return 1;
  if (u != 15) return 2;
  return 0;
}
