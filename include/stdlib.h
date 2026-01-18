#ifndef C99CC_STDLIB_H
#define C99CC_STDLIB_H

#include <stddef.h>

struct c99cc_div_t {
  int quot;
  int rem;
};
typedef struct c99cc_div_t div_t;

struct c99cc_ldiv_t {
  long quot;
  long rem;
};
typedef struct c99cc_ldiv_t ldiv_t;

int atoi(const char* s);
long atol(const char* s);
long long atoll(const char* s);

void* malloc(size_t size);
void* calloc(size_t count, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

void abort(void);
void exit(int status);

int abs(int v);
long labs(long v);
long long llabs(long long v);

div_t div(int num, int den);
ldiv_t ldiv(long num, long den);

#endif
