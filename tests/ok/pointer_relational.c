// EXPECT: 1
int main() {
  int x;
  int *p = &x;
  if (p <= &x && !(p < &x) && !(p > &x) && (p >= &x)) return 1;
  return 0;
}
