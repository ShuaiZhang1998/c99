int main() {
  int a[1 + 2];
  a[2] = 7;
  return a[2];
}
// EXPECT: 7
