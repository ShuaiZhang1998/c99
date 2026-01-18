// EXPECT: 4
#if (1 << 2) == 4 && ((8 >> 1) == 4)
int main() { return 4; }
#else
int main() { return 0; }
#endif
