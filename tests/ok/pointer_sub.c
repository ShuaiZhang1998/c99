// EXPECT: 0
int main() {
  int x;
  int *p = &x;
  int *q = &x;
  return p - q;
}
