// ARGS: -I include
// EXPECT: 0
#include <stdlib.h>

int main() {
  if (atoi("  -42") != -42) return 1;
  if (atol("+15") != 15) return 2;
  if (atoll("7") != 7) return 3;
  if (abs(-3) != 3) return 4;
  if (labs(-5) != 5) return 5;
  if (llabs(-6) != 6) return 6;
  div_t d = div(7, 3);
  if (d.quot != 2 || d.rem != 1) return 7;
  ldiv_t ld = ldiv(10, 4);
  if (ld.quot != 2 || ld.rem != 2) return 8;
  return 0;
}
