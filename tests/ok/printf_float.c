// ARGS: -I include
// EXPECT: 8
int printf(const char* fmt, ...);

int main() {
  return printf("%f", 1.25);
}
