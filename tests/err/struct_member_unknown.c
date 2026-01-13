// ERROR: unknown field 'c' in struct 'S'
struct S { int a; };
int main() {
  struct S s;
  return s.c;
}
