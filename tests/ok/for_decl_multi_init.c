// EXPECT: 5
int main() {
  int sum = 0;
  for (int i = 0, j = 2; i < 2; i = i + 1, j = j + 1) {
    sum = sum + j;
  }
  return sum;
}
