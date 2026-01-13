// EXPECT: 3
struct S { int a; int b; };

int main() {
  struct S s = {1, 2};
  return s.a + s.b;
}
