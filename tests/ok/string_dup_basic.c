// ARGS: -I include
// EXPECT: 0
#include <string.h>
#include <stdlib.h>

int main() {
  const char* src = "hello";
  char* dup = strdup(src);
  if (!dup) return 1;
  if (dup == src) return 2;
  if (strcmp(dup, src) != 0) return 3;
  dup[0] = 'y';
  if (strcmp(dup, "yello") != 0) return 4;
  if (strcmp(src, "hello") != 0) return 5;
  free(dup);

  dup = strdup("");
  if (!dup) return 6;
  if (strcmp(dup, "") != 0) return 7;
  free(dup);

  return 0;
}
