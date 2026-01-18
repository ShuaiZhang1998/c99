// EXPECT: 1
int main() {
  int a[2];
  int* p = a;
  ++p;
  return p - a;
}
