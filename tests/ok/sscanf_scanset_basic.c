// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  char out[8];
  if (sscanf("abc123", "%[a-z]", out) != 1) return 1;
  if (strcmp(out, "abc") != 0) return 2;
  return 0;
}
