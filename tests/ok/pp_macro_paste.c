// EXPECT: 5
#define MK(a, b) a##b

int xy = 5;

int main() {
  return MK(x, y);
}
