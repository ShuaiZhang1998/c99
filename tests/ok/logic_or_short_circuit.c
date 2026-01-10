// EXPECT: 0
int main() {
  int x = 0;
  if (1 || (x = x + 1)) { }
  return x;
}
