// ERROR: invalid operands to relational operator
int main() {
  int x;
  int *p = &x;
  return p < 1;
}
