// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  int a = 0;
  double d = 0.0;
  int n = sscanf("12 9.876", "%2d %4lf", &a, &d);
  if (n != 2) return 1;
  if (a != 12) return 2;
  if (d < 9.86 || d > 9.88) return 3;
  return 0;
}
