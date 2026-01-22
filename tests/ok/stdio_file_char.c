// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp_char.txt", "w");
  if (!f) return 1;
  if (fputc('A', f) == EOF) return 2;
  if (fputs("BC", f) == EOF) return 3;
  if (fclose(f) != 0) return 4;

  f = fopen("c99cc_stdio_tmp_char.txt", "r");
  if (!f) return 5;
  if (fgetc(f) != 'A') return 6;
  if (fgetc(f) != 'B') return 7;
  if (fgetc(f) != 'C') return 8;
  if (fgetc(f) != EOF) return 9;
  if (!feof(f)) return 10;
  fclose(f);
  return 0;
}
