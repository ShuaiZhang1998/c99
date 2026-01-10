// EXPECT: 6
int main() {
  int x = 0;
  int i = 0;
  while (i < 3) {
    x = x + 2;
    i = i + 1;
  }
  return x;
}
