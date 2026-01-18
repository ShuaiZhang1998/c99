// ERROR: array designator out of range
int main() {
  int a[2] = { [2] = 1 };
  return 0;
}
