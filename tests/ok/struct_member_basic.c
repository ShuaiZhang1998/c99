// EXPECT: 7
struct S { int a; int b; };

int main() {
  struct S s;
  s.a = 3;
  s.b = 4;
  return s.a + s.b;
}
