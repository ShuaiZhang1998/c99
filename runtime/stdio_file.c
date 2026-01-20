#define C99CC_STDIO_IMPL
#include "stdio.h"
#include "stdlib.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static FILE std_files[3];
FILE* stdin = &std_files[0];
FILE* stdout = &std_files[1];
FILE* stderr = &std_files[2];
static int std_inited = 0;

void c99cc_init_stdio(void) {
  if (std_inited) return;
#ifdef _WIN32
  std_files[0].handle = GetStdHandle(STD_INPUT_HANDLE);
  std_files[1].handle = GetStdHandle(STD_OUTPUT_HANDLE);
  std_files[2].handle = GetStdHandle(STD_ERROR_HANDLE);
#else
  std_files[0].fd = 0;
  std_files[1].fd = 1;
  std_files[2].fd = 2;
#endif
  std_inited = 1;
}

FILE** c99cc_stdin_ptr(void) {
  c99cc_init_stdio();
  return &stdin;
}

FILE** c99cc_stdout_ptr(void) {
  c99cc_init_stdio();
  return &stdout;
}

FILE** c99cc_stderr_ptr(void) {
  c99cc_init_stdio();
  return &stderr;
}

int c99cc_write_file(FILE* f, const unsigned char* buf, size_t len) {
  if (!f) return -1;
#ifdef _WIN32
  DWORD written = 0;
  if (!WriteFile(f->handle, buf, (DWORD)len, &written, NULL)) return -1;
  return (int)written;
#else
  ssize_t n = write(f->fd, buf, len);
  if (n < 0) return -1;
  return (int)n;
#endif
}

int c99cc_read_file(FILE* f, unsigned char* buf, size_t len) {
  if (!f) return -1;
#ifdef _WIN32
  DWORD read_bytes = 0;
  if (!ReadFile(f->handle, buf, (DWORD)len, &read_bytes, NULL)) return -1;
  return (int)read_bytes;
#else
  ssize_t n = read(f->fd, buf, len);
  if (n < 0) return -1;
  return (int)n;
#endif
}

static int parse_mode(const char* mode, int* out_plus) {
  if (!mode || !mode[0]) return -1;
  *out_plus = 0;
  for (const char* p = mode; *p; ++p) {
    if (*p == '+') *out_plus = 1;
  }
  return mode[0];
}

FILE* fopen(const char* path, const char* mode) {
  if (!path || !mode) return NULL;
  int plus = 0;
  int m = parse_mode(mode, &plus);
  if (m < 0) return NULL;
#ifdef _WIN32
  DWORD access = 0;
  DWORD create_disp = OPEN_EXISTING;
  if (m == 'r') {
    access = plus ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
    create_disp = OPEN_EXISTING;
  } else if (m == 'w') {
    access = plus ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_WRITE;
    create_disp = CREATE_ALWAYS;
  } else if (m == 'a') {
    access = plus ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_WRITE;
    create_disp = OPEN_ALWAYS;
  } else {
    return NULL;
  }
  HANDLE h = CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         create_disp, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) return NULL;
  if (m == 'a') {
    SetFilePointer(h, 0, NULL, FILE_END);
  }
  FILE* f = (FILE*)malloc(sizeof(FILE));
  if (!f) return NULL;
  f->handle = h;
  f->flags = 0;
  return f;
#else
  int flags = 0;
  if (m == 'r') {
    flags = plus ? O_RDWR : O_RDONLY;
  } else if (m == 'w') {
    flags = plus ? (O_RDWR | O_CREAT | O_TRUNC) : (O_WRONLY | O_CREAT | O_TRUNC);
  } else if (m == 'a') {
    flags = plus ? (O_RDWR | O_CREAT | O_APPEND) : (O_WRONLY | O_CREAT | O_APPEND);
  } else {
    return NULL;
  }
  int fd = open(path, flags, 0644);
  if (fd < 0) return NULL;
  FILE* f = (FILE*)malloc(sizeof(FILE));
  if (!f) return NULL;
  f->fd = fd;
  f->flags = 0;
  return f;
#endif
}

int fclose(FILE* f) {
  if (!f) return -1;
  c99cc_init_stdio();
  if (f == stdin || f == stdout || f == stderr) return 0;
#ifdef _WIN32
  CloseHandle(f->handle);
#else
  close(f->fd);
#endif
  free(f);
  return 0;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* f) {
  if (!ptr || !f || size == 0 || nmemb == 0) return 0;
  size_t total = size * nmemb;
  if (total / size != nmemb) return 0;
  int n = c99cc_read_file(f, (unsigned char*)ptr, total);
  if (n <= 0) return 0;
  return (size_t)n / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f) {
  if (!ptr || !f || size == 0 || nmemb == 0) return 0;
  size_t total = size * nmemb;
  if (total / size != nmemb) return 0;
  int n = c99cc_write_file(f, (const unsigned char*)ptr, total);
  if (n <= 0) return 0;
  return (size_t)n / size;
}
