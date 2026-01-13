// EXPECT: 7
int main() {
  int x = 0;
  int y = 0;
  switch (x) {
    default:
      y = 7;
  }
  return y;
}
