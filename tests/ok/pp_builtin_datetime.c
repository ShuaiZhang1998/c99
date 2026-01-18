// EXPECT: 1
int main() {
  return (__DATE__[0] > 0) && (__TIME__[0] > 0);
}
