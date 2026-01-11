// EXPECT: 3
int main() {
  int x = 0;
  for (; x < 5; ) {
    x = x + 1;
    if (x < 3) continue;
    return 3;
  }
  return 0;
}
