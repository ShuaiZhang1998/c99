// EXPECT: 3
int main() {
#if 0
  return 1;
#elif 0
  return 2;
#else
  return 3;
#endif
}
