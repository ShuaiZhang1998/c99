// EXPECT: 5
struct S { int a; };

int main() {
  struct S s;
  struct S *p;
  p = &s;
  p->a = 5;
  return s.a;
}
