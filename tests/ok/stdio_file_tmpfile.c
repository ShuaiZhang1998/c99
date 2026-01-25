// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  FILE* f = tmpfile();
  if (!f) return 1;
  if (fwrite("abc", 1, 3, f) != 3) return 2;
  if (fseek(f, 0, SEEK_SET) != 0) return 3;
  char buf[4];
  size_t n = fread(buf, 1, 3, f);
  buf[n] = '\0';
  if (strcmp(buf, "abc") != 0) return 4;
  fclose(f);
  return 0;
}
