// EXPECT: 7
int main() {
  int x = 7;
  for (int i = 0; i < 3; i = i + 1) {
    x = x + 0;
  }
  return x;
}
