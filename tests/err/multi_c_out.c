// ARGS: tests/fixtures/multi_helper.c -c -o out.o
// ERROR: error: -o with -c requires a single input file
int main() { return 0; }
