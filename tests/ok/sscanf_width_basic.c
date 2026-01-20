// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  char buf[4];
  int n = sscanf("abcdef", "%3s", buf);
  if (n != 1) return 1;
  if (strcmp(buf, "abc") != 0) return 2;
  return 0;
}
