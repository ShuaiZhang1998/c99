// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp_ungetc_eof.txt", "w");
  if (!f) return 1;
  if (fclose(f) != 0) return 2;

  f = fopen("c99cc_stdio_tmp_ungetc_eof.txt", "r");
  if (!f) return 3;
  if (ungetc('Z', f) == EOF) return 4;
  char buf[2];
  size_t n = fread(buf, 1, 2, f);
  if (n != 1) return 5;
  if (!feof(f)) return 6;
  fclose(f);
  return 0;
}
