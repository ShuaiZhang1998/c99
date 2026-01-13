// EXPECT: 4
int get2(int *p) { return p[1]; }

int main() {
  int a[2];
  a[0] = 3;
  a[1] = 4;
  return get2(a);
}
