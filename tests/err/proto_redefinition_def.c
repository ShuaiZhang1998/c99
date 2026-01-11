// ERROR: redefinition of 'add'
int add(int a, int b) { return a + b; }
int add(int a, int b) { return a + b + 1; }

int main() { return 0; }
