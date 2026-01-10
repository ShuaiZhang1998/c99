// EXPECT: 5
int main() {
  int x = 1;
  if (x) { x = x + 4; }
  return x;
}
