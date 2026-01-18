// ARGS: -I include
// EXPECT: 11
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

int main() {
  size_t a = 4;
  ptrdiff_t b = -1;
  uint32_t c = 3;
  int8_t d = 2;
  bool ok = true;
  return (int)(a + c + d + (ok ? 1 : 0) + (b < 0 ? 1 : 0));
}
