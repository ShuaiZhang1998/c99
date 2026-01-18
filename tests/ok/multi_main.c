// ARGS: tests/fixtures/multi_helper.c
// EXPECT: 7
int add2(int a, int b);
int main() {
  return add2(3, 4);
}
