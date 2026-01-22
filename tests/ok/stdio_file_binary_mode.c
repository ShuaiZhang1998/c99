// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp_bin.txt", "wb");
  if (!f) return 1;
  if (fwrite("ab", 1, 2, f) != 2) return 2;
  if (fclose(f) != 0) return 3;

  f = fopen("c99cc_stdio_tmp_bin.txt", "ab");
  if (!f) return 4;
  if (fwrite("cd", 1, 2, f) != 2) return 5;
  if (fclose(f) != 0) return 6;

  f = fopen("c99cc_stdio_tmp_bin.txt", "rb");
  if (!f) return 7;
  char buf[8];
  size_t n = fread(buf, 1, 4, f);
  buf[n] = '\0';
  fclose(f);
  if (strcmp(buf, "abcd") != 0) return 8;
  return 0;
}
