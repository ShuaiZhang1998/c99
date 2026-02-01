// EXPECT: 7

union U { int i; char c; };

int main() {
  union U u = { .i = 7 };
  return u.i;
}
