// ERROR: invalid operands to equality operator
int main() {
  int x;
  int *p = &x;
  int **pp = &p;
  return p == pp;
}
