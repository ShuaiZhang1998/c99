// EXPECT: 5

union U { int i; char c; };

int main() {
  union U u;
  u.i = 5;
  union U* p = &u;
  return p->i;
}
