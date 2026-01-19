// ARGS: -I include
// EXPECT: 3
#include <stdio.h>

int main() {
  int a = putchar('A');
  int b = puts("B");
  return a + b;
}
