// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  double a = 0.0;
  double b = 0.0;
  if (sscanf("1.25e2 3.5", "%le %lg", &a, &b) != 2) return 1;
  if ((int)a != 125) return 2;
  if ((int)(b * 10) != 35) return 3;
  return 0;
}
