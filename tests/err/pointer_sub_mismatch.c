// ERROR: invalid operands to pointer arithmetic
int main() {
  int x;
  int *p = &x;
  int **pp = &p;
  return p - pp;
}
