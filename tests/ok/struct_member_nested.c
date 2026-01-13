// EXPECT: 8
struct Inner { int x; };
struct Outer { struct Inner in; int arr[2]; };

int main() {
  struct Outer o;
  struct Inner i;
  i.x = 3;
  o.in = i;
  o.arr[1] = 5;
  return o.in.x + o.arr[1];
}
