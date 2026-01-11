// EXPECT: 5
int main() {
  int i = 0;
  int sum = 0;
  for (i = 0; i < 5; i = i + 1) {
    sum = sum + 1;
  }
  return sum;
}
