#ifndef C99CC_ASSERT_H
#define C99CC_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

void __c99cc_assert_fail(const char* expr, const char* file, int line);

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) ((void)((expr) || (__c99cc_assert_fail(#expr, __FILE__, __LINE__), 0)))
#endif

#endif
