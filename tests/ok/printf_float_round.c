// ARGS: -I include
// EXPECT: 4
int printf(const char* fmt, ...);

int main() {
  return printf("%.2f", 1.999);
}
