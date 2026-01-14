// EXPECT: 3
#define INC(x) ((x) + 1)
#define ADD2(x) INC(INC(x))
int main() {
  return ADD2(1);
}
