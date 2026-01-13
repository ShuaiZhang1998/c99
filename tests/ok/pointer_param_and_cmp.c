// EXPECT: 1
int set(int *p) {
  *p = 7;
  return 0;
}

int main() {
  int x;
  int *p = 0;
  int *q = &x;
  set(&x);
  p = &x;
  if (p == 0) return 0;
  if (p == q) return 1;
  return 0;
}
