// EXPECT: 1
// -(1+2) * 3 = -9, !(-9) = 0, but we want: !(0) => 1
int main() { return !(0); }
