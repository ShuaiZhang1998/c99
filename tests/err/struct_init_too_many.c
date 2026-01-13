// ERROR: excess elements in struct initializer
struct S { int a; };
int main() {
  struct S s = {1, 2};
  return 0;
}
