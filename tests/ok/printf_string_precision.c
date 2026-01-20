// ARGS: -I include
// EXPECT: 8
int printf(const char* fmt, ...);

int main() {
  int a = printf("%.3s", "abcdef");
  int b = printf("%5.3s", "abcdef");
  return a + b;
}
