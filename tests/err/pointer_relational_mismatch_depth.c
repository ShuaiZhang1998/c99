// ERROR: invalid operands to relational operator
int main() {
  int x;
  int *p = &x;
  int **pp = &p;
  return p < pp;
}
