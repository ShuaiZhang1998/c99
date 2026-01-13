// EXPECT: 7
struct Inner { int x; };
struct Outer { struct Inner in; int arr[2]; };

int main() {
  struct Outer o = {{3}, {4, 5}};
  return o.in.x + o.arr[0];
}
