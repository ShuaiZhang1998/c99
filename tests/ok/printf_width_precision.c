// ARGS: -I include
// EXPECT: 16
int printf(const char* fmt, ...);

int main() {
  int a = printf("%.2f", 1.234);
  int b = printf("%4d", 12);
  int c = printf("%8.2f", 1.2);
  return a + b + c;
}
