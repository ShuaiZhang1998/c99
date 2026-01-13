// ERROR: cannot assign to array
struct S { int arr[2]; };
int main() {
  struct S a;
  struct S b;
  a.arr = b.arr;
  return 0;
}
