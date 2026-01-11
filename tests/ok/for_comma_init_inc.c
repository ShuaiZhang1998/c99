// EXPECT: 10
int main() {
  int i = 0;
  int sum = 0;
  for (i = 0, sum = 0; i < 4; i = i + 1, sum = sum + i) {
  }
  return sum; // 1+2+3+4=10
}
