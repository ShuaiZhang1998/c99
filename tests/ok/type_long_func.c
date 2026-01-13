// EXPECT: 6
long add(long a, long b) {
  return a + b;
}

int main() {
  long x = add(1, 2);
  return x + 3;
}
