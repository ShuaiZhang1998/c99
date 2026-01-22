// ARGS: -I include
// EXPECT: 0
#include <stdlib.h>

int main() {
  char* end = 0;
  double a = strtod("1.25e2", &end);
  if ((int)a != 125) return 1;
  if (*end != '\0') return 2;

  double b = strtod("3.5", &end);
  if ((int)(b * 10) != 35) return 3;
  return 0;
}
