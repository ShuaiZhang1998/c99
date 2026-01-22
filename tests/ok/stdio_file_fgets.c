// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp_fgets.txt", "w");
  if (!f) return 1;
  if (fputs("hello\nworld", f) == EOF) return 2;
  if (fclose(f) != 0) return 3;

  f = fopen("c99cc_stdio_tmp_fgets.txt", "r");
  if (!f) return 4;
  char buf[16];
  if (!fgets(buf, sizeof(buf), f)) return 5;
  if (strcmp(buf, "hello\n") != 0) return 6;
  if (!fgets(buf, sizeof(buf), f)) return 7;
  if (strcmp(buf, "world") != 0) return 8;
  fclose(f);
  return 0;
}
