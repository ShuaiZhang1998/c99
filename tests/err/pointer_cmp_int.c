// ERROR: invalid operands to equality operator
int main() {
  int x;
  int *p = &x;
  return p == 1;
}
