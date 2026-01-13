// ERROR: duplicate default label
int main() {
  switch (0) {
    default:
      break;
    default:
      break;
  }
  return 0;
}
