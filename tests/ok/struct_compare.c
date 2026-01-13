// EXPECT: 1
struct S { int a; int b; };

int main() {
  struct S a = {1, 2};
  struct S b = {1, 2};
  return a == b;
}
