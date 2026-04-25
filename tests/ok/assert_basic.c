// ARGS: -I include
// EXPECT: 0
#include <assert.h>

int main() {
  assert(1);
  assert(2 + 3 == 5);
  return 0;
}
