// EXPECT: 9
int add(int a, int b);
int add(int x, int y);

int add(int a, int b) {
  return a + b;
}

int main() {
  return add(4, 5);
}
