// EXPECT: 6
struct S { int a; int b; };

int sum(struct S s) { return s.a + s.b; }

int main() {
  struct S v;
  v.a = 2;
  v.b = 4;
  return sum(v);
}
