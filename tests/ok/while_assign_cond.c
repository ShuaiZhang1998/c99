// EXPECT: 4
int main() {
  int x = 0;
  while ((x = x + 1) < 4) { }
  return x;
}
