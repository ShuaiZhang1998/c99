// ERROR: expected expression
int main() {
  int x = 0;
  x = (1,);
  return x;
}
