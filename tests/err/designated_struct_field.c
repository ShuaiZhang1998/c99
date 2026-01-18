// ERROR: unknown field 'x' in struct 'S'
struct S { int a; };
int main() {
  struct S s = { .x = 1 };
  return 0;
}
