#define C99CC_STDIO_IMPL
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
extern int mkstemp(char* template);
#endif

#define C99CC_FILE_EOF 0x1
#define C99CC_FILE_ERR 0x2

static FILE std_files[3];
FILE* stdin = &std_files[0];
FILE* stdout = &std_files[1];
FILE* stderr = &std_files[2];
static int std_inited = 0;

static void c99cc_set_errno(int code) {
  if (code != 0) errno = code;
}

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
  std_files[0].flags = 0;
  std_files[1].flags = 0;
  std_files[2].flags = 0;
  std_files[0].has_ungot = 0;
  std_files[1].has_ungot = 0;
  std_files[2].has_ungot = 0;
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

static int parse_mode(const char* mode, int* out_plus, int* out_binary) {
  if (!mode || !mode[0]) return -1;
  *out_plus = 0;
  *out_binary = 0;
  for (const char* p = mode; *p; ++p) {
    if (*p == '+') *out_plus = 1;
    if (*p == 'b') *out_binary = 1;
  }
  return mode[0];
}

FILE* fopen(const char* path, const char* mode) {
  if (!path || !mode) return NULL;
  int plus = 0;
  int binary = 0;
  int m = parse_mode(mode, &plus, &binary);
  if (m < 0) return NULL;
#ifdef _WIN32
  DWORD access = 0;
  DWORD create_disp = OPEN_EXISTING;
  DWORD attrs = FILE_ATTRIBUTE_NORMAL;
  if (!binary) attrs |= FILE_ATTRIBUTE_NORMAL;
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
                         create_disp, attrs, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    c99cc_set_errno(EIO);
    return NULL;
  }
  if (m == 'a') {
    SetFilePointer(h, 0, NULL, FILE_END);
  }
  FILE* f = (FILE*)malloc(sizeof(FILE));
  if (!f) {
    CloseHandle(h);
    c99cc_set_errno(ENOMEM);
    return NULL;
  }
  f->has_ungot = 0;
  f->ungot = 0;
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
  if (fd < 0) {
    c99cc_set_errno(ENOENT);
    return NULL;
  }
  FILE* f = (FILE*)malloc(sizeof(FILE));
  if (!f) {
    close(fd);
    c99cc_set_errno(ENOMEM);
    return NULL;
  }
  f->has_ungot = 0;
  f->ungot = 0;
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
  if (!CloseHandle(f->handle)) c99cc_set_errno(EIO);
#else
  if (close(f->fd) != 0) c99cc_set_errno(EIO);
#endif
  free(f);
  return 0;
}

int remove(const char* path) {
  if (!path) return -1;
#ifdef _WIN32
  if (DeleteFileA(path)) return 0;
  c99cc_set_errno(ENOENT);
  return -1;
#else
  if (unlink(path) == 0) return 0;
  c99cc_set_errno(ENOENT);
  return -1;
#endif
}

int rename(const char* oldpath, const char* newpath) {
  if (!oldpath || !newpath) return -1;
#ifdef _WIN32
  if (MoveFileA(oldpath, newpath)) return 0;
  c99cc_set_errno(ENOENT);
  return -1;
#else
  unlink(newpath);
  if (link(oldpath, newpath) != 0) {
    c99cc_set_errno(ENOENT);
    return -1;
  }
  if (unlink(oldpath) != 0) {
    unlink(newpath);
    c99cc_set_errno(EIO);
    return -1;
  }
  return 0;
#endif
}

char* tmpnam(char* s) {
  static char buf[L_tmpnam];
  char* out = s ? s : buf;
#ifdef _WIN32
  char tmp_path[MAX_PATH];
  DWORD n = GetTempPathA(MAX_PATH, tmp_path);
  if (n == 0 || n > MAX_PATH) {
    c99cc_set_errno(EIO);
    return NULL;
  }
  char tmp_file[MAX_PATH];
  if (GetTempFileNameA(tmp_path, "c99", 0, tmp_file) == 0) {
    c99cc_set_errno(EIO);
    return NULL;
  }
  DeleteFileA(tmp_file);
  size_t i = 0;
  while (tmp_file[i] && i + 1 < L_tmpnam) {
    out[i] = tmp_file[i];
    i++;
  }
  out[i] = '\0';
  return out;
#else
  const char* prefix = "/tmp/c99ccXXXXXX";
  size_t i = 0;
  while (prefix[i] && i + 1 < L_tmpnam) {
    out[i] = prefix[i];
    i++;
  }
  out[i] = '\0';
  int fd = mkstemp(out);
  if (fd < 0) {
    c99cc_set_errno(EIO);
    return NULL;
  }
  close(fd);
  unlink(out);
  return out;
#endif
}

FILE* tmpfile(void) {
#ifdef _WIN32
  char tmp_path[MAX_PATH];
  DWORD n = GetTempPathA(MAX_PATH, tmp_path);
  if (n == 0 || n > MAX_PATH) {
    c99cc_set_errno(EIO);
    return NULL;
  }
  char tmp_file[MAX_PATH];
  if (GetTempFileNameA(tmp_path, "c99", 0, tmp_file) == 0) {
    c99cc_set_errno(EIO);
    return NULL;
  }
  HANDLE h = CreateFileA(tmp_file, GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    c99cc_set_errno(EIO);
    return NULL;
  }
  FILE* f = (FILE*)malloc(sizeof(FILE));
  if (!f) {
    CloseHandle(h);
    c99cc_set_errno(ENOMEM);
    return NULL;
  }
  f->has_ungot = 0;
  f->ungot = 0;
  f->handle = h;
  f->flags = 0;
  return f;
#else
  char name[] = "/tmp/c99ccXXXXXX";
  int fd = mkstemp(name);
  if (fd < 0) {
    c99cc_set_errno(EIO);
    return NULL;
  }
  unlink(name);
  FILE* f = (FILE*)malloc(sizeof(FILE));
  if (!f) {
    close(fd);
    c99cc_set_errno(ENOMEM);
    return NULL;
  }
  f->has_ungot = 0;
  f->ungot = 0;
  f->fd = fd;
  f->flags = 0;
  return f;
#endif
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* f) {
  if (!ptr || !f || size == 0 || nmemb == 0) return 0;
  size_t total = size * nmemb;
  if (total / size != nmemb) return 0;
  size_t off = 0;
  if (f->has_ungot && total > 0) {
    ((unsigned char*)ptr)[0] = (unsigned char)f->ungot;
    f->has_ungot = 0;
    off = 1;
  }
  int n = 0;
  if (total > off) {
    n = c99cc_read_file(f, (unsigned char*)ptr + off, total - off);
    if (n < 0) {
      f->flags |= C99CC_FILE_ERR;
      c99cc_set_errno(EIO);
      return 0;
    }
    if (n < (int)(total - off)) f->flags |= C99CC_FILE_EOF;
    if (n == 0 && off == 0) return 0;
  }
  size_t got = off + (size_t)n;
  if (got == 0) {
    f->flags |= C99CC_FILE_ERR;
    return 0;
  }
  if (got < total && off == 0) f->flags |= C99CC_FILE_EOF;
  return got / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f) {
  if (!ptr || !f || size == 0 || nmemb == 0) return 0;
  size_t total = size * nmemb;
  if (total / size != nmemb) return 0;
  int n = c99cc_write_file(f, (const unsigned char*)ptr, total);
  if (n < 0) {
    f->flags |= C99CC_FILE_ERR;
    return 0;
  }
  return (size_t)n / size;
}

int fgetc(FILE* f) {
  if (!f) return EOF;
  if (f->has_ungot) {
    f->has_ungot = 0;
    return f->ungot;
  }
  unsigned char ch = 0;
  int n = c99cc_read_file(f, &ch, 1);
  if (n < 0) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return EOF;
  }
  if (n == 0) {
    f->flags |= C99CC_FILE_EOF;
    return EOF;
  }
  return (int)ch;
}

int fputc(int c, FILE* f) {
  if (!f) return EOF;
  unsigned char ch = (unsigned char)c;
  int n = c99cc_write_file(f, &ch, 1);
  if (n != 1) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return EOF;
  }
  return (int)ch;
}

int ungetc(int c, FILE* f) {
  if (!f || c == EOF) return EOF;
  if (f->has_ungot) return EOF;
  f->has_ungot = 1;
  f->ungot = (unsigned char)c;
  f->flags &= ~C99CC_FILE_EOF;
  return (unsigned char)c;
}

int getc(FILE* f) {
  return fgetc(f);
}

int putc(int c, FILE* f) {
  return fputc(c, f);
}

char* fgets(char* s, int n, FILE* f) {
  if (!s || !f || n <= 0) return NULL;
  int i = 0;
  for (; i < n - 1; ++i) {
    int ch = fgetc(f);
    if (ch == EOF) break;
    s[i] = (char)ch;
    if (ch == '\n') {
      ++i;
      break;
    }
  }
  if (i == 0) return NULL;
  s[i] = '\0';
  return s;
}

int fputs(const char* s, FILE* f) {
  if (!s || !f) return EOF;
  size_t len = 0;
  while (s[len]) ++len;
  if (len == 0) return 0;
  size_t n = fwrite(s, 1, len, f);
  if (n != len) return EOF;
  return (int)n;
}

int fseek(FILE* f, long offset, int whence) {
  if (!f) return -1;
#ifdef _WIN32
  LARGE_INTEGER li;
  li.QuadPart = offset;
  DWORD method = FILE_BEGIN;
  if (whence == SEEK_CUR) method = FILE_CURRENT;
  else if (whence == SEEK_END) method = FILE_END;
  LARGE_INTEGER out;
  if (!SetFilePointerEx(f->handle, li, &out, method)) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return -1;
  }
#else
  off_t r = lseek(f->fd, (off_t)offset, whence);
  if (r == (off_t)-1) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return -1;
  }
#endif
  f->flags &= ~C99CC_FILE_EOF;
  f->has_ungot = 0;
  return 0;
}

long ftell(FILE* f) {
  if (!f) return -1;
#ifdef _WIN32
  LARGE_INTEGER zero;
  zero.QuadPart = 0;
  LARGE_INTEGER out;
  if (!SetFilePointerEx(f->handle, zero, &out, FILE_CURRENT)) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return -1;
  }
  return (long)out.QuadPart;
#else
  off_t r = lseek(f->fd, 0, SEEK_CUR);
  if (r == (off_t)-1) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return -1;
  }
  return (long)r;
#endif
}

int fseeko(FILE* f, long long offset, int whence) {
  if (!f) return -1;
#ifdef _WIN32
  LARGE_INTEGER li;
  li.QuadPart = offset;
  DWORD method = FILE_BEGIN;
  if (whence == SEEK_CUR) method = FILE_CURRENT;
  else if (whence == SEEK_END) method = FILE_END;
  LARGE_INTEGER out;
  if (!SetFilePointerEx(f->handle, li, &out, method)) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return -1;
  }
#else
  off_t r = lseek(f->fd, (off_t)offset, whence);
  if (r == (off_t)-1) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return -1;
  }
#endif
  f->flags &= ~C99CC_FILE_EOF;
  f->has_ungot = 0;
  return 0;
}

long long ftello(FILE* f) {
  if (!f) return -1;
#ifdef _WIN32
  LARGE_INTEGER zero;
  zero.QuadPart = 0;
  LARGE_INTEGER out;
  if (!SetFilePointerEx(f->handle, zero, &out, FILE_CURRENT)) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return -1;
  }
  return (long long)out.QuadPart;
#else
  off_t r = lseek(f->fd, 0, SEEK_CUR);
  if (r == (off_t)-1) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return -1;
  }
  return (long long)r;
#endif
}

void rewind(FILE* f) {
  if (!f) return;
  fseek(f, 0, SEEK_SET);
  clearerr(f);
}

int fflush(FILE* f) {
  if (!f) return 0;
#ifdef _WIN32
  if (!FlushFileBuffers(f->handle)) {
    f->flags |= C99CC_FILE_ERR;
    c99cc_set_errno(EIO);
    return EOF;
  }
#endif
  return 0;
}

int feof(FILE* f) {
  if (!f) return 0;
  return (f->flags & C99CC_FILE_EOF) != 0;
}

int ferror(FILE* f) {
  if (!f) return 0;
  return (f->flags & C99CC_FILE_ERR) != 0;
}

void clearerr(FILE* f) {
  if (!f) return;
  f->flags &= ~(C99CC_FILE_EOF | C99CC_FILE_ERR);
}

void perror(const char* s) {
  c99cc_init_stdio();
  const char* msg = s ? s : "";
  if (msg[0] != '\0') {
    c99cc_write_file(stderr, (const unsigned char*)msg, (size_t)strlen(msg));
    c99cc_write_file(stderr, (const unsigned char*)": ", 2);
  }
  if (errno == 0) {
    c99cc_write_file(stderr, (const unsigned char*)"error\n", 6);
    return;
  }
  char buf[32];
  int v = errno;
  int i = 0;
  if (v < 0) {
    buf[i++] = '-';
    v = -v;
  }
  char digits[16];
  int d = 0;
  while (v > 0 && d < 16) {
    digits[d++] = (char)('0' + (v % 10));
    v /= 10;
  }
  if (d == 0) digits[d++] = '0';
  while (d > 0) buf[i++] = digits[--d];
  buf[i++] = '\n';
  c99cc_write_file(stderr, (const unsigned char*)"error ", 6);
  c99cc_write_file(stderr, (const unsigned char*)buf, (size_t)i);
}
