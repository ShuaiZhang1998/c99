// ARGS: -I include
// EXPECT: 0
#include <stdio.h>

int main() {
  char name1[L_tmpnam];
  char name2[L_tmpnam];
  if (!tmpnam(name1)) return 1;
  if (!tmpnam(name2)) return 2;

  FILE* f = fopen(name1, "w");
  if (!f) return 3;
  if (fputs("hi", f) == EOF) return 4;
  if (fclose(f) != 0) return 5;

  if (rename(name1, name2) != 0) return 6;
  f = fopen(name1, "r");
  if (f) {
    fclose(f);
    return 7;
  }
  f = fopen(name2, "r");
  if (!f) return 8;
  fclose(f);
  if (remove(name2) != 0) return 9;
  f = fopen(name2, "r");
  if (f) {
    fclose(f);
    return 10;
  }
  return 0;
}
