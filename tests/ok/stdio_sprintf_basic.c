// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  char buf[32];
  int n = sprintf(buf, "x=%d %s", 7, "ok");
  if (n != 6) return 1;
  if (buf[0] != 'x') return 2;
  if (buf[1] != '=') return 3;
  if (buf[2] != '7') return 4;
  if (buf[3] != ' ') return 5;
  if (buf[4] != 'o') return 6;
  if (buf[5] != 'k') return 7;
  if (buf[6] != '\0') return 8;
  return 0;
}
