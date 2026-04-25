// ARGS: -I include
// EXPECT: 0
#include <string.h>

int main() {
  const char* s = "abcabc";

  if (memchr(s, 'b', 6) != s + 1) return 1;
  if (memchr(s, 'z', 6) != 0) return 2;
  if (strchr(s, 'c') != s + 2) return 3;
  if (strchr(s, '\0') != s + 6) return 4;
  if (strrchr(s, 'a') != s + 3) return 5;
  if (strrchr(s, 'z') != 0) return 6;
  if (strstr(s, "cab") != s + 2) return 7;
  if (strstr(s, "") != s) return 8;
  if (strstr(s, "zzz") != 0) return 9;

  return 0;
}
