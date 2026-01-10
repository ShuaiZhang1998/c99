// EXPECT: 3
int main() {
  int x = 0;
  while (1) {
    x = x + 1;
    if (x == 3) break;
  }
  return x;
}
