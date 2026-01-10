// EXPECT: 3
int main() {
  int x = 0;
  int y = 0;
  while (x < 5) {
    x = x + 1;
    if (x < 3) continue;
    y = y + 1;
  }
  return y; // x=1,2 continue; x=3,4,5 -> y=3
}
