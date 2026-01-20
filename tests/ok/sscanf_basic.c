// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  int a = 0;
  char s[8];
  char c = 0;
  int n = sscanf("  -12 abc X", "%d %s %c", &a, s, &c);
  if (n != 3) return 1;
  if (a != -12) return 2;
  if (strcmp(s, "abc") != 0) return 3;
  if (c != 'X') return 4;
  return 0;
}
