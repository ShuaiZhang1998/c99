// ARGS: -I include
// EXPECT: 0
#include <stdlib.h>

int main() {
  char* path = getenv("PATH");
  if (!path) return 1;
  if (*path == '\0') return 2;
  if (getenv("C99CC_SHOULD_NOT_EXIST_123456789") != 0) return 3;
  return 0;
}
