// EXPECT: 7
struct S { int a; int b; };

struct S make(int x, int y) {
  struct S s;
  s.a = x;
  s.b = y;
  return s;
}

int main() {
  struct S v;
  v = make(3, 4);
  return v.a + v.b;
}
