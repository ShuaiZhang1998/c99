// EXPECT: 5
enum Color { RED = 1, GREEN, BLUE = 5 };

int main() {
  enum Color c = BLUE;
  return c;
}
