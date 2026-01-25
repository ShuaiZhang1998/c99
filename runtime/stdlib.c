#include <stdlib.h>
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

static int is_space_char(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int digit_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
  return -1;
}

long strtol(const char* nptr, char** endptr, int base) {
  const char* s = nptr;
  if (!s) {
    if (endptr) *endptr = (char*)nptr;
    return 0;
  }
  while (is_space_char(*s)) ++s;
  int sign = 1;
  if (*s == '+' || *s == '-') {
    if (*s == '-') sign = -1;
    ++s;
  }
  if (base == 0) {
    if (*s == '0') {
      if (s[1] == 'x' || s[1] == 'X') base = 16;
      else base = 8;
    } else {
      base = 10;
    }
  }
  if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
  unsigned long value = 0;
  int any = 0;
  while (*s) {
    int d = digit_value(*s);
    if (d < 0 || d >= base) break;
    value = value * (unsigned long)base + (unsigned long)d;
    any = 1;
    ++s;
  }
  if (endptr) *endptr = (char*)(any ? s : nptr);
  return (long)(sign * (long)value);
}

unsigned long strtoul(const char* nptr, char** endptr, int base) {
  const char* s = nptr;
  if (!s) {
    if (endptr) *endptr = (char*)nptr;
    return 0;
  }
  while (is_space_char(*s)) ++s;
  int sign = 1;
  if (*s == '+' || *s == '-') {
    if (*s == '-') sign = -1;
    ++s;
  }
  if (base == 0) {
    if (*s == '0') {
      if (s[1] == 'x' || s[1] == 'X') base = 16;
      else base = 8;
    } else {
      base = 10;
    }
  }
  if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
  unsigned long value = 0;
  int any = 0;
  while (*s) {
    int d = digit_value(*s);
    if (d < 0 || d >= base) break;
    value = value * (unsigned long)base + (unsigned long)d;
    any = 1;
    ++s;
  }
  if (endptr) *endptr = (char*)(any ? s : nptr);
  if (sign < 0) return (unsigned long)(-(long)value);
  return value;
}

double strtod(const char* nptr, char** endptr) {
  const char* s = nptr;
  if (!s) {
    if (endptr) *endptr = (char*)nptr;
    return 0.0;
  }
  while (is_space_char(*s)) ++s;
  int sign = 1;
  if (*s == '+' || *s == '-') {
    if (*s == '-') sign = -1;
    ++s;
  }
  double value = 0.0;
  int any = 0;
  while (*s >= '0' && *s <= '9') {
    value = value * 10.0 + (double)(*s - '0');
    any = 1;
    ++s;
  }
  if (*s == '.') {
    ++s;
    double scale = 0.1;
    while (*s >= '0' && *s <= '9') {
      value += (double)(*s - '0') * scale;
      scale *= 0.1;
      any = 1;
      ++s;
    }
  }
  int exp = 0;
  int exp_sign = 1;
  const char* exp_start = s;
  if (*s == 'e' || *s == 'E') {
    ++s;
    if (*s == '+' || *s == '-') {
      if (*s == '-') exp_sign = -1;
      ++s;
    }
    if (*s < '0' || *s > '9') {
      s = exp_start;
    } else {
      while (*s >= '0' && *s <= '9') {
        exp = exp * 10 + (*s - '0');
        ++s;
      }
    }
  }
  if (endptr) *endptr = (char*)(any ? s : nptr);
  if (!any) return 0.0;
  if (exp != 0) {
    int e = exp_sign * exp;
    if (e > 0) {
      while (e-- > 0) value *= 10.0;
    } else {
      while (e++ < 0) value *= 0.1;
    }
  }
  return (double)sign * value;
}

static unsigned long rand_state = 1;

void srand(unsigned int seed) {
  rand_state = seed ? seed : 1u;
}

int rand(void) {
  rand_state = rand_state * 1103515245u + 12345u;
  return (int)((rand_state >> 16) & 0x7fff);
}

static void swap_bytes(unsigned char* a, unsigned char* b, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    unsigned char tmp = a[i];
    a[i] = b[i];
    b[i] = tmp;
  }
}

static void insertion_sort(unsigned char* base, size_t nmemb, size_t size,
                           int (*compar)(const void*, const void*)) {
  for (size_t i = 1; i < nmemb; ++i) {
    size_t j = i;
    while (j > 0 && compar(base + (j - 1) * size, base + j * size) > 0) {
      swap_bytes(base + (j - 1) * size, base + j * size, size);
      --j;
    }
  }
}

static void qsort_impl(unsigned char* base, size_t nmemb, size_t size,
                       int (*compar)(const void*, const void*)) {
  if (nmemb < 2) return;
  if (nmemb < 16) {
    insertion_sort(base, nmemb, size, compar);
    return;
  }
  unsigned char* pivot = (unsigned char*)malloc(size);
  if (!pivot) {
    insertion_sort(base, nmemb, size, compar);
    return;
  }
  mem_copy(pivot, base + (nmemb / 2) * size, size);

  size_t i = 0;
  size_t j = nmemb - 1;
  while (1) {
    while (compar(base + i * size, pivot) < 0) i++;
    while (compar(base + j * size, pivot) > 0) {
      if (j == 0) break;
      j--;
    }
    if (i >= j) break;
    swap_bytes(base + i * size, base + j * size, size);
    i++;
    if (j == 0) break;
    j--;
  }
  free(pivot);

  size_t left = j + 1;
  if (left > 0) qsort_impl(base, left, size, compar);
  if (left < nmemb) qsort_impl(base + left * size, nmemb - left, size, compar);
}

void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*)) {
  if (!base || nmemb == 0 || size == 0 || !compar) return;
  qsort_impl((unsigned char*)base, nmemb, size, compar);
}

void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*)) {
  if (!key || !base || nmemb == 0 || size == 0 || !compar) return NULL;
  size_t lo = 0;
  size_t hi = nmemb;
  const unsigned char* b = (const unsigned char*)base;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    const void* elem = b + mid * size;
    int c = compar(key, elem);
    if (c == 0) return (void*)elem;
    if (c < 0) hi = mid;
    else lo = mid + 1;
  }
  return NULL;
}
