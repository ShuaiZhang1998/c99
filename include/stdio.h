#ifndef C99CC_STDIO_H
#define C99CC_STDIO_H

#include <stddef.h>

typedef struct c99cc_FILE {
#ifdef _WIN32
  void* handle;
#else
  int fd;
#endif
  int flags;
} FILE;

FILE** c99cc_stdin_ptr(void);
FILE** c99cc_stdout_ptr(void);
FILE** c99cc_stderr_ptr(void);

#ifndef C99CC_STDIO_IMPL
#define stdin (*c99cc_stdin_ptr())
#define stdout (*c99cc_stdout_ptr())
#define stderr (*c99cc_stderr_ptr())
#endif

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* f);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* f);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f);
int fprintf(FILE* f, const char* fmt, ...);
int sprintf(char* s, const char* fmt, ...);
int snprintf(char* s, size_t n, const char* fmt, ...);

int printf(const char* fmt, ...);
int putchar(int c);
int puts(const char* s);

#endif
