// EXPECT: 6
#define ADD1(a, ...) ((a) + (__VA_ARGS__))

int main() {
  return ADD1(1, 2 + 3);
}
