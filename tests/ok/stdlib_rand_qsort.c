// ARGS: -I include
// EXPECT: 0
#include <stdlib.h>

int cmp_int(const void* a, const void* b) {
  int av = *(int*)a;
  int bv = *(int*)b;
  if (av < bv) return -1;
  if (av > bv) return 1;
  return 0;
}

int main() {
  srand(1);
  int r1 = rand();
  int r2 = rand();
  if (r1 != 16838) return 1;
  if (r2 != 5758) return 2;

  int arr[2] = {2, 1};
  qsort(arr, 2, sizeof(int), cmp_int);
  if (arr[0] != 1 || arr[1] != 2)
    return 3;

  int key = 2;
  int* found = (int*)bsearch(&key, arr, 2, sizeof(int), cmp_int);
  if (!found || *found != 2) return 4;
  return 0;
}
