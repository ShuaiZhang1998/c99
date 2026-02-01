// ERROR: expected constant expression after 'case'
int main() {
  int x = 1;
  switch (x) {
    case x:
      break;
  }
  return 0;
}
