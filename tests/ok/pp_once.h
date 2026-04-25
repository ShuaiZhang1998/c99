#pragma once

enum { PP_ONCE_VALUE = 7 };

static int pp_once_value(void) {
  return PP_ONCE_VALUE;
}
