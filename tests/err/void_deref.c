// ERROR: cannot dereference void pointer
int main() {
  void *p = NULL;
  return *p;
}
