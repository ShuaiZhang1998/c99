// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  double v = 0.0;
  char c = 0;
  int n = sscanf("1e+X", "%f%c", &v, &c);
  return (n == 2 && c == 'e') ? 0 : 1;
}
