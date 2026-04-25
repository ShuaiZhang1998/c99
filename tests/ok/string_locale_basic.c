// ARGS: -I include
// EXPECT: 0
#include <string.h>

int main() {
  char buf[8];

  if (strcoll("abc", "abc") != 0) return 1;
  if (strcoll("abc", "abd") >= 0) return 2;
  if (strcoll("abd", "abc") <= 0) return 3;

  if (strxfrm(buf, "abc", sizeof(buf)) != 3) return 4;
  if (strcmp(buf, "abc") != 0) return 5;

  memset(buf, 'x', sizeof(buf));
  if (strxfrm(buf, "abcdef", 4) != 6) return 6;
  if (memcmp(buf, "abc\0", 4) != 0) return 7;

  memset(buf, 'x', sizeof(buf));
  if (strxfrm(buf, "abc", 0) != 3) return 8;
  if (buf[0] != 'x') return 9;

  return 0;
}
