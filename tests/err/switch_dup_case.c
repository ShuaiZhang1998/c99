// ERROR: duplicate case value '1'
int main() {
  switch (1) {
    case 1:
      break;
    case 1:
      break;
  }
  return 0;
}
