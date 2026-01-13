// EXPECT: 3
int main() {
  int x = 2;
  int y = 0;
  switch (x) {
    case 1:
      y = 1;
      break;
    case 2:
      y = 3;
      break;
    default:
      y = 9;
  }
  return y;
}
