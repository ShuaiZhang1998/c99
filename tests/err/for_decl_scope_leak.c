// ERROR: use of undeclared identifier 'i'
int main() {
  for (int i = 0; i < 2; i = i + 1) { }
  return i;
}
