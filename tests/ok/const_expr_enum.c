enum { A = 1, B = A + 2, C = B * 2 };
int main() {
  return C;
}
// EXPECT: 6
