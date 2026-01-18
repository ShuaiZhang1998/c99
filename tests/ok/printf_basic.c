// ARGS: -I include
// EXPECT: 9
#include <stdio.h>

int main() {
  int a = printf("hi");
  int b = printf(" %d", 1234);
  int c = printf(" %c", 'x');
  return a + b + c;
}
