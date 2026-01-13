// EXPECT: 1
struct S { int arr[2]; };

int main() {
  struct S a = {{1, 2}};
  struct S b = {{1, 3}};
  return a != b;
}
