// ERROR: redefinition of 'struct S'
struct S { int a; };
struct S { int b; };
int main() { return 0; }
