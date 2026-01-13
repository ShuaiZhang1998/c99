// ERROR: expected integer literal after 'case'
int main() {
  int x = 1;
  switch (x) {
    case x:
      break;
  }
  return 0;
}
