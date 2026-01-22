// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp_ungetc_limit.txt", "w");
  if (!f) return 1;
  if (fputs("x", f) == EOF) return 2;
  if (fclose(f) != 0) return 3;

  f = fopen("c99cc_stdio_tmp_ungetc_limit.txt", "r");
  if (!f) return 4;
  if (ungetc('a', f) == EOF) return 5;
  if (ungetc('b', f) != EOF) return 6;
  if (getc(f) != 'a') return 7;
  if (getc(f) != 'x') return 8;
  fclose(f);
  return 0;
}
