// ERROR: switch condition must be int
int main() {
  float x = 1.0f;
  switch (x) { case 1: return 1; }
}
