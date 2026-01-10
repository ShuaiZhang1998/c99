// EXPECT: 7
int main() {
  int x = 0;
  while (1) {
    x = x + 1;
    if (x == 7) return x;
  }
  return 0;
}
