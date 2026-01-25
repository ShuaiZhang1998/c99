// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <string.h>

int main() {
  FILE* f = fopen("c99cc_stdio_tmp_fscanf.txt", "w");
  if (!f) return 1;
  if (fprintf(f, "12 34 test") <= 0) return 2;
  if (fclose(f) != 0) return 3;

  f = fopen("c99cc_stdio_tmp_fscanf.txt", "r");
  if (!f) return 4;
  int a = 0;
  int b = 0;
  char s[8];
  int n = fscanf(f, "%d %d %s", &a, &b, s);
  fclose(f);
  if (n != 3) return 5;
  if (a != 12 || b != 34) return 6;
  if (strcmp(s, "test") != 0) return 7;
  return 0;
}
