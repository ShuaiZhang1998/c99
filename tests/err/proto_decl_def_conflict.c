// ERROR: conflicting types for 'add'
int add(int);

int add(int a, int b) { return a + b; }

int main() { return 0; }
