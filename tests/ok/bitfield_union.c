// EXPECT: 7

union U {
  unsigned a:3;
  unsigned b:5;
};

int main() {
  union U u;
  u.b = 7;
  return u.b;
}
