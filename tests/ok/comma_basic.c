// EXPECT: 5
int main() {
  int x = 0;
  int y = 0;
  return (x = 3, y = 5, x + (y - 3));
}
