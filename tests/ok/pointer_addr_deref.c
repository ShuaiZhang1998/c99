// EXPECT: 3
int main() {
  int x;
  int *p;
  p = &x;
  *p = 3;
  return x;
}
