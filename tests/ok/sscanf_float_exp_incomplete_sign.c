// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  double v = 0.0;
  char c1 = 0;
  char c2 = 0;
  int n = sscanf("1e+X", "%lf%c%c", &v, &c1, &c2);
  return (n == 3 && c1 == 'e' && c2 == '+') ? 0 : 1;
}
