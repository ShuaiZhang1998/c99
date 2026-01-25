int main() {
  unsigned int a = 1u;
  unsigned long b = 2ul;
  unsigned long long c = 3ull;
  long long d = 4ll;
  return (int)(a + b + c + d);
}
// EXPECT: 10
