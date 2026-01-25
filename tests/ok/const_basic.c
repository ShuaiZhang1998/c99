int main() {
  const int x = 1;
  int y = x + 2;
  int a = 3;
  const int *p = &x;
  const int *q = &a;
  int *r = &a;
  const int *s = r;
  int * const t = r;
  return y + *p + *q + *s + *t;
}
// EXPECT: 13
