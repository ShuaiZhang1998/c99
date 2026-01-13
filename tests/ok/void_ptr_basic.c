// EXPECT: 1
int main() {
  int x;
  void *p = NULL;
  p = &x;
  return p == &x;
}
