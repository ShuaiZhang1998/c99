// EXPECT: 3
int inc(int x) {
  return x + 1;
}

int main() {
  inc(1);
  return inc(2);
}
