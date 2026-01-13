// EXPECT: 6
int get(int a[][3]) { return a[1][2]; }

int main() {
  int a[2][3];
  a[1][2] = 6;
  return get(a);
}
