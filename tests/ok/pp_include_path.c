// ARGS: -I tests/include
// EXPECT: 42
#include "pp_path.h"

int main() {
  return path_inc_value();
}
