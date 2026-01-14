// EXPECT: 1
#define A

int main() {
#ifdef A
  return 1;
#else
  return 2;
#endif
}
