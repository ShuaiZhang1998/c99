// ARGS: -I include
// EXPECT: 0
#include <stdlib.h>

int main() {
  int* p = (int*)calloc(3, sizeof(int));
  if (!p) return 1;
  if (p[0] != 0 || p[1] != 0 || p[2] != 0) return 2;
  p[0] = 7;
  p[1] = 9;
  p = (int*)realloc(p, 5 * sizeof(int));
  if (!p) return 3;
  if (p[0] != 7 || p[1] != 9) return 4;
  free(p);
  return 0;
}
