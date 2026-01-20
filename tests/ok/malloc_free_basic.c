// ARGS: -I include
// EXPECT: 6
#include <stdlib.h>

int main() {
  int* p = (int*)malloc(3 * sizeof(int));
  if (!p) return 1;
  p[0] = 1;
  p[1] = 2;
  p[2] = 3;
  int sum = p[0] + p[1] + p[2];
  free(p);
  return sum;
}
