// ARGS: -I include
// EXPECT: 0
#include <string.h>

int main() {
  char buf[16];
  memset(buf, 'a', 3);
  buf[3] = '\0';
  if (strlen(buf) != 3) return 1;
  if (strcmp(buf, "aaa") != 0) return 2;

  char dst[16];
  strcpy(dst, "hi");
  if (strcmp(dst, "hi") != 0) return 3;
  strcat(dst, "!");
  if (strcmp(dst, "hi!") != 0) return 4;

  char nbuf[16];
  strncpy(nbuf, "abcd", 2);
  nbuf[2] = '\0';
  if (strcmp(nbuf, "ab") != 0) return 5;
  strncat(nbuf, "xyz", 1);
  if (strcmp(nbuf, "abx") != 0) return 6;

  char mv[16];
  strcpy(mv, "abcd");
  memmove(mv + 1, mv, 5);
  if (strcmp(mv, "aabcd") != 0) return 7;
  return 0;
}
