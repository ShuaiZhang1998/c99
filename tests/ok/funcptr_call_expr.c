// EXPECT: 0

int add1(int x) { return x + 1; }
int add2(int x) { return x + 2; }

int main() {
  int (*fp)(int) = add1;
  if (fp(1) != 2) return 1;
  if ((*fp)(2) != 3) return 2;

  int (*arr[2])(int) = {add1, add2};
  if (arr[0](3) != 4) return 3;
  if (arr[1](3) != 5) return 4;
  return 0;
}
