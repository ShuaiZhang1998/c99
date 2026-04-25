// ARGS: -I include
// EXPECT: 0
#include <string.h>

int main() {
  const char* s = "abc123xyz";

  if (strspn(s, "abc") != 3) return 1;
  if (strspn(s, "") != 0) return 2;
  if (strspn(s, "abc123xyz") != 9) return 3;

  if (strcspn(s, "123") != 3) return 4;
  if (strcspn(s, "z") != 8) return 5;
  if (strcspn(s, "") != 9) return 6;

  if (strpbrk(s, "31") != s + 3) return 7;
  if (strpbrk(s, "zq") != s + 8) return 8;
  if (strpbrk(s, "QWERT") != 0) return 9;

  return 0;
}
