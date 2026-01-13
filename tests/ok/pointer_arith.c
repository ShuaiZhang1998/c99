// EXPECT: 1
int main() {
  int x;
  int *p = &x;
  p = p + 1;
  p = p - 1;
  return p == &x;
}
