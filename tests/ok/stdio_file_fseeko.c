// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp_fseeko.txt", "w");
  if (!f) return 1;
  if (fwrite("abcdefghij", 1, 10, f) != 10) return 2;
  if (fclose(f) != 0) return 3;

  f = fopen("c99cc_stdio_tmp_fseeko.txt", "r");
  if (!f) return 4;
  if (fseeko(f, 4, SEEK_SET) != 0) return 5;
  if (fgetc(f) != 'e') return 6;
  if (ftello(f) != 5) return 7;
  fclose(f);
  return 0;
}
