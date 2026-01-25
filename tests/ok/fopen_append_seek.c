// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  const char* path = "c99cc_stdio_tmp_append_seek.txt";
  FILE* f = fopen(path, "w");
  if (!f) return 1;
  fputs("abc", f);
  fclose(f);

  f = fopen(path, "a+");
  if (!f) return 2;
  long pos = ftell(f);
  if (pos != 3) {
    fclose(f);
    return 3;
  }
  fclose(f);
  return 0;
}
