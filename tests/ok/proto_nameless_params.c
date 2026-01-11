// EXPECT: 6
int add(int, int);

int add(int a, int b) {
  return a + b;
}

int main() {
  return add(1, 5);
}
