// EXPECT: 5
int main() {
  int x;
  int *p;
  int **pp;
  p = &x;
  pp = &p;
  **pp = 5;
  return x;
}
