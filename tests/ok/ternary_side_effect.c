// EXPECT: 2
int main() {
  int x = 0;
  int y = 0;
  y = (1 ? (x = 2) : (x = 9));
  return x;
}
