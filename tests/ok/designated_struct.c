// EXPECT: 6
struct S { int a; int b; int c; };
int main() {
  struct S s = { .b = 2, .a = 1, .c = 3 };
  return s.a + s.b + s.c;
}
