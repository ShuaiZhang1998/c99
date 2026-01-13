// ERROR: invalid operands to equality operator
struct A { int a; };
struct B { int a; };
int main() {
  struct A a;
  struct B b;
  return a == b;
}
