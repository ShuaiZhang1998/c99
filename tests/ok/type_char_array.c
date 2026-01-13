// EXPECT: 3
int main() {
  char a[3];
  a[0] = 1;
  a[1] = 2;
  a[2] = 3;
  char *p = a;
  return p[2];
}
