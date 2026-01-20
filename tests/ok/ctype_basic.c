// ARGS: -I include
// EXPECT: 0
#include <ctype.h>

int main() {
  if (!isdigit('7')) return 1;
  if (isdigit('a')) return 2;
  if (!isspace(' ')) return 3;
  if (!isspace('\n')) return 4;
  if (isspace('x')) return 5;
  return 0;
}
