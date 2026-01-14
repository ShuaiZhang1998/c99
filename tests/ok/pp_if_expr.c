// EXPECT: 1
int main() {
#if (1 + 2 * 3) == 7 && !0
  return 1;
#else
  return 0;
#endif
}
