// ERROR: invalid operands to equality operator
int main() {
  void **pp;
  int **qq;
  return pp == qq;
}
