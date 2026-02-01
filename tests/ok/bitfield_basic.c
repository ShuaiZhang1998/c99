// EXPECT: 55

struct S {
  unsigned a:3;
  unsigned b:5;
  unsigned c:6;
};

int main() {
  struct S s;
  s.a = 5;
  s.b = 17;
  s.c = 33;
  return s.a + s.b + s.c;
}
