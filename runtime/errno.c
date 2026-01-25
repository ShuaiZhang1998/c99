#include "errno.h"

static int c99cc_errno_value = 0;

int* __c99cc_errno(void) {
  return &c99cc_errno_value;
}
