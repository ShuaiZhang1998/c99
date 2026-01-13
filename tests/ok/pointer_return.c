// EXPECT: 1
int *id(int *p) { return p; }

int main() {
  int x;
  int *p = id(&x);
  return p == &x;
}
