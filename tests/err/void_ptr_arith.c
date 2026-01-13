// ERROR: invalid operands to pointer arithmetic
int main() {
  void *p = NULL;
  p = p + 1;
  return 0;
}
