// EXPECT: 0
int main() {
  int x = 3;
  while (x) x = x - 1;
  return x;
}
