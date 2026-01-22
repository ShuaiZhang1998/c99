// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp_seek.txt", "w");
  if (!f) return 1;
  if (fwrite("abcdef", 1, 6, f) != 6) return 2;
  if (fclose(f) != 0) return 3;

  f = fopen("c99cc_stdio_tmp_seek.txt", "r");
  if (!f) return 4;
  if (fseek(f, 2, SEEK_SET) != 0) return 5;
  if (fgetc(f) != 'c') return 6;
  if (ftell(f) != 3) return 7;
  if (fseek(f, -2, SEEK_END) != 0) return 8;
  if (fgetc(f) != 'e') return 9;
  rewind(f);
  if (fgetc(f) != 'a') return 10;
  fclose(f);
  return 0;
}
