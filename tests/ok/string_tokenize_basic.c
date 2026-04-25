// ARGS: -I include
// EXPECT: 0
#include <string.h>

int main() {
  char buf[] = ",a::bc,";
  char* tok;

  tok = strtok(buf, ",:");
  if (!tok || strcmp(tok, "a") != 0) return 1;

  tok = strtok(0, ",:");
  if (!tok || strcmp(tok, "bc") != 0) return 2;

  tok = strtok(0, ",:");
  if (tok != 0) return 3;

  tok = strtok(buf, ",:");
  if (!tok || strcmp(tok, "a") != 0) return 4;

  return 0;
}
