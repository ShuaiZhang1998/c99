// EXPECT: 2
int main() {
  int x = 0;
  do {
    x = x + 1;
    if (x == 2) break;
  } while (1);
  return x;
}
