// EXPECT: 3
int main() {
  int x = 0;
  for (x = 0; ; x = x + 1) {
    if (x == 3) break;
  }
  return x;
}
