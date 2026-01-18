// EXPECT: 6
int main() {
  int a[5] = { [1] = 2, [3] = 4 };
  return a[1] + a[3];
}
