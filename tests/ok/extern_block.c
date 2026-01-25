int g = 4;
int main() {
  extern int g;
  return g;
}
// EXPECT: 4
