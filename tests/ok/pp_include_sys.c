// ARGS: -isystem tests/sysinclude
// EXPECT: 21
#include <sys_header.h>

int main() {
  return sys_inc_value();
}
