// EXPECT: 3
int main() {
  int x = 0;
  do {
    x = x + 1;
    continue;
    x = 999;
  } while (x < 3);
  return x;
}
