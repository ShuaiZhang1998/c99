// EXPECT: 1
#define FLAG 1
int main() {
#if defined(FLAG)
  return 1;
#else
  return 0;
#endif
}
