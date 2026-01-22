int main() {
  int x = 1;
  int *q = &x;
  int * const p = q;
  p = q;
  return 0;
}
// ERROR: cannot assign to const object
