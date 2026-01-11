// ERROR: expected ')'
int main() {
  int x = 0;
  do { x = x + 1; } while (x < 3;
  return x;
}
