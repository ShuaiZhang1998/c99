// EXPECT: 9
struct S { int a; int b; };

int main() {
  struct S a;
  struct S b;
  a.a = 4;
  a.b = 5;
  b = a;
  return b.a + b.b;
}
