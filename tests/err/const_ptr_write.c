int main() {
  int x = 1;
  const int *p = &x;
  *p = 2;
  return 0;
}
// ERROR: cannot assign to const object
