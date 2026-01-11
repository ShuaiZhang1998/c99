// EXPECT: 9
int add(int a, int b) {
  return a + b;
}

int main() {
  return add(add(1, 2), 6);
}
