// ERROR: invalid #if expression
#define BAD abc
int main() {
#if BAD
  return 1;
#endif
  return 0;
}
