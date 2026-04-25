// EXPECT: 0
#include "pp_once.h"
#include "pp_once.h"

int main() {
  return pp_once_value() == 7 ? 0 : 1;
}
