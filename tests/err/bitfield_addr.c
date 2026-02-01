// ERROR: cannot take address of bit-field

struct S { unsigned a:3; };

int main() {
  struct S s;
  return (int)(&s.a != 0);
}
