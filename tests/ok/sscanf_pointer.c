// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <stdint.h>

int main() {
  void* p = 0;
  if (sscanf("10", "%p", &p) != 1) return 1;
  if ((uintptr_t)p != (uintptr_t)16) return 2;
  return 0;
}
