#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

typedef struct {
  size_t size;
  size_t total;
} BlockHeader;

typedef struct {
  int quot;
  int rem;
} div_t;

typedef struct {
  long quot;
  long rem;
} ldiv_t;

static size_t round_up(size_t n, size_t align) {
  return (n + align - 1) / align * align;
}

static size_t page_size(void) {
#ifdef _WIN32
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return (size_t)info.dwPageSize;
#else
  long ps = sysconf(_SC_PAGESIZE);
  if (ps <= 0) return 4096;
  return (size_t)ps;
#endif
}

static void* alloc_pages(size_t size) {
#ifdef _WIN32
  return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
#define MAP_ANON MAP_ANONYMOUS
#endif
  void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == (void*)-1) return NULL;
  return p;
#endif
}

static void free_pages(void* base, size_t size) {
#ifdef _WIN32
  (void)size;
  VirtualFree(base, 0, MEM_RELEASE);
#else
  munmap(base, size);
#endif
}

static void mem_set(unsigned char* dst, unsigned char v, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    dst[i] = v;
  }
}

static void mem_copy(unsigned char* dst, const unsigned char* src, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    dst[i] = src[i];
  }
}

void* malloc(size_t size) {
  if (size == 0) size = 1;
  size_t ps = page_size();
  size_t total = round_up(sizeof(BlockHeader) + size, ps);
  void* base = alloc_pages(total);
  if (!base) return NULL;
  BlockHeader* h = (BlockHeader*)base;
  h->size = size;
  h->total = total;
  return (void*)((unsigned char*)base + sizeof(BlockHeader));
}

void free(void* ptr) {
  if (!ptr) return;
  BlockHeader* h = (BlockHeader*)((unsigned char*)ptr - sizeof(BlockHeader));
  free_pages(h, h->total);
}

void* calloc(size_t count, size_t size) {
  if (count == 0 || size == 0) {
    return malloc(1);
  }
  size_t total = count * size;
  if (total / size != count) {
    return NULL;
  }
  unsigned char* p = (unsigned char*)malloc(total);
  if (!p) return NULL;
  mem_set(p, 0, total);
  return p;
}

void* realloc(void* ptr, size_t size) {
  if (!ptr) return malloc(size);
  if (size == 0) {
    free(ptr);
    return NULL;
  }
  BlockHeader* h = (BlockHeader*)((unsigned char*)ptr - sizeof(BlockHeader));
  size_t old = h->size;
  if (size <= old) return ptr;
  unsigned char* p = (unsigned char*)malloc(size);
  if (!p) return NULL;
  mem_copy(p, (unsigned char*)ptr, old);
  free(ptr);
  return p;
}

void exit(int status) {
#ifdef _WIN32
  ExitProcess((unsigned int)status);
#else
  _exit(status);
#endif
}

void abort(void) {
#ifdef _WIN32
  ExitProcess(134);
#else
  _exit(134);
#endif
}

int abs(int v) {
  return v < 0 ? -v : v;
}

long labs(long v) {
  return v < 0 ? -v : v;
}

long long llabs(long long v) {
  return v < 0 ? -v : v;
}

int atoi(const char* s) {
  if (!s) return 0;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' ||
         *s == '\v') {
    ++s;
  }
  int sign = 1;
  if (*s == '+' || *s == '-') {
    if (*s == '-') sign = -1;
    ++s;
  }
  int v = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    ++s;
  }
  return sign * v;
}

long atol(const char* s) {
  if (!s) return 0;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' ||
         *s == '\v') {
    ++s;
  }
  long sign = 1;
  if (*s == '+' || *s == '-') {
    if (*s == '-') sign = -1;
    ++s;
  }
  long v = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    ++s;
  }
  return sign * v;
}

long long atoll(const char* s) {
  if (!s) return 0;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' ||
         *s == '\v') {
    ++s;
  }
  long long sign = 1;
  if (*s == '+' || *s == '-') {
    if (*s == '-') sign = -1;
    ++s;
  }
  long long v = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    ++s;
  }
  return sign * v;
}

div_t div(int num, int den) {
  div_t r;
  r.quot = num / den;
  r.rem = num % den;
  return r;
}

ldiv_t ldiv(long num, long den) {
  ldiv_t r;
  r.quot = num / den;
  r.rem = num % den;
  return r;
}
