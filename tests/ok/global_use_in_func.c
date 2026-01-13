// EXPECT: 2
int g = 1;

int inc() {
  g = g + 1;
  return g;
}

int main() {
  return inc();
}
