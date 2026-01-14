// EXPECT: 7
#define PICK(a, ...) (a)

int main() {
  return PICK(7);
}
