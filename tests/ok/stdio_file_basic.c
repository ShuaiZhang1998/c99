// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp.txt", "w");
  if (!f) return 1;
  if (fprintf(f, "hi %d", 3) != 4) return 2;
  if (fwrite("!", 1, 1, f) != 1) return 3;
  if (fclose(f) != 0) return 4;

  f = fopen("c99cc_stdio_tmp.txt", "r");
  if (!f) return 5;
  char buf[8];
  size_t n = fread(buf, 1, 5, f);
  buf[n] = '\0';
  fclose(f);
  if (strcmp(buf, "hi 3!") != 0) return 6;

  char out[4];
  int r = snprintf(out, sizeof(out), "%d", 12345);
  if (r != 5) return 7;
  if (strcmp(out, "123") != 0) return 8;
  return 0;
}
