// EXPECT: 0
int accept_cmp(int (*cmp)(const void*, const void*)) {
  (void)cmp;
  return 0;
}

int main(void) {
  return accept_cmp(0);
}
