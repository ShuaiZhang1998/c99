// ARGS: -I include
// EXPECT: 0
#include <stdio.h>
#include <errno.h>

int main() {
  errno = ENOENT;
  perror("open");
  return 0;
}
