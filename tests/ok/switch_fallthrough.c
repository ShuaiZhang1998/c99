// EXPECT: 6
int main() {
  int x = 1;
  int y = 0;
  switch (x) {
    case 1:
      y = y + 1;
    case 2:
      y = y + 5;
      break;
    default:
      y = 0;
  }
  return y;
}
