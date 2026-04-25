// ARGS: -I include
// EXPECT: 7
#define NDEBUG
#include <assert.h>

int main() {
  int x = 7;
  assert(x == 0);
  return x;
}
