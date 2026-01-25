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
  int has_ungot;
  int ungot;
} FILE;

#ifndef EOF
#define EOF (-1)
#endif

#ifndef L_tmpnam
#define L_tmpnam 260
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

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
int fgetc(FILE* f);
int fputc(int c, FILE* f);
int ungetc(int c, FILE* f);
int getc(FILE* f);
int putc(int c, FILE* f);
char* fgets(char* s, int n, FILE* f);
int fputs(const char* s, FILE* f);
int fseek(FILE* f, long offset, int whence);
long ftell(FILE* f);
int fseeko(FILE* f, long long offset, int whence);
long long ftello(FILE* f);
void rewind(FILE* f);
int fflush(FILE* f);
int feof(FILE* f);
int ferror(FILE* f);
void clearerr(FILE* f);
int fprintf(FILE* f, const char* fmt, ...);
int sprintf(char* s, const char* fmt, ...);
int snprintf(char* s, size_t n, const char* fmt, ...);
int scanf(const char* fmt, ...);
int sscanf(const char* s, const char* fmt, ...);
int fscanf(FILE* f, const char* fmt, ...);
int remove(const char* path);
int rename(const char* oldpath, const char* newpath);
char* tmpnam(char* s);
FILE* tmpfile(void);
void perror(const char* s);

int printf(const char* fmt, ...);
int putchar(int c);
int puts(const char* s);

#endif
