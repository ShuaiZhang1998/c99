// EXPECT: 3
#if (1 | 2) == 3 && (7 ^ 3) == 4 && (7 & 3) == 3
int main() { return 3; }
#else
int main() { return 0; }
#endif
