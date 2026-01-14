// EXPECT: 46
#define NUM(a, b) a##b

int main() {
  return NUM(1, 2) + NUM(3, 4);
}
