// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp_ungetc.txt", "w");
  if (!f) return 1;
  if (putc('a', f) == EOF) return 2;
  if (putc('b', f) == EOF) return 3;
  if (fclose(f) != 0) return 4;

  f = fopen("c99cc_stdio_tmp_ungetc.txt", "r");
  if (!f) return 5;
  int ch = getc(f);
  if (ch != 'a') return 6;
  if (ungetc(ch, f) == EOF) return 7;
  if (getc(f) != 'a') return 8;
  if (fgetc(f) != 'b') return 9;
  fclose(f);
  return 0;
}
