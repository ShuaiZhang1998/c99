int main() {
  int x = 3;
  switch (x) {
    case 1 + 2: return 5;
    default: return 1;
  }
}
// EXPECT: 5
