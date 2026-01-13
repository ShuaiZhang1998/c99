// EXPECT: 1
int isnull(void *p) { return p == NULL; }

int main() {
  return isnull(NULL);
}
