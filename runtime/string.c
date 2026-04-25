#include <stddef.h>
#include "errno.h"
#include "stdlib.h"

void* memcpy(void* dst, const void* src, size_t n) {
  unsigned char* d = (unsigned char*)dst;
  const unsigned char* s = (const unsigned char*)src;
  for (size_t i = 0; i < n; ++i) {
    d[i] = s[i];
  }
  return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
  unsigned char* d = (unsigned char*)dst;
  const unsigned char* s = (const unsigned char*)src;
  if (d == s || n == 0) return dst;
  if (d < s) {
    for (size_t i = 0; i < n; ++i) {
      d[i] = s[i];
    }
  } else {
    for (size_t i = n; i > 0; --i) {
      d[i - 1] = s[i - 1];
    }
  }
  return dst;
}

void* memset(void* dst, int c, size_t n) {
  unsigned char* d = (unsigned char*)dst;
  unsigned char v = (unsigned char)c;
  for (size_t i = 0; i < n; ++i) {
    d[i] = v;
  }
  return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
  const unsigned char* x = (const unsigned char*)a;
  const unsigned char* y = (const unsigned char*)b;
  for (size_t i = 0; i < n; ++i) {
    if (x[i] != y[i]) {
      return (x[i] < y[i]) ? -1 : 1;
    }
  }
  return 0;
}

void* memchr(const void* s, int c, size_t n) {
  const unsigned char* p = (const unsigned char*)s;
  unsigned char want = (unsigned char)c;
  for (size_t i = 0; i < n; ++i) {
    if (p[i] == want) return (void*)(p + i);
  }
  return 0;
}

size_t strlen(const char* s) {
  size_t n = 0;
  while (s[n]) {
    n++;
  }
  return n;
}

char* strcpy(char* dst, const char* src) {
  char* out = dst;
  while (*src) {
    *dst++ = *src++;
  }
  *dst = '\0';
  return out;
}

char* strncpy(char* dst, const char* src, size_t n) {
  char* out = dst;
  size_t i = 0;
  for (; i < n && src[i]; ++i) {
    dst[i] = src[i];
  }
  for (; i < n; ++i) {
    dst[i] = '\0';
  }
  return out;
}

int strcmp(const char* a, const char* b) {
  while (*a && *b) {
    if (*a != *b) {
      return (*a < *b) ? -1 : 1;
    }
    ++a;
    ++b;
  }
  if (*a == *b) return 0;
  return (*a < *b) ? -1 : 1;
}

int strncmp(const char* a, const char* b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    unsigned char ca = (unsigned char)a[i];
    unsigned char cb = (unsigned char)b[i];
    if (ca != cb) return (ca < cb) ? -1 : 1;
    if (ca == 0) return 0;
  }
  return 0;
}

char* strcat(char* dst, const char* src) {
  char* out = dst;
  while (*dst) dst++;
  while (*src) {
    *dst++ = *src++;
  }
  *dst = '\0';
  return out;
}

char* strncat(char* dst, const char* src, size_t n) {
  char* out = dst;
  while (*dst) dst++;
  size_t i = 0;
  for (; i < n && src[i]; ++i) {
    dst[i] = src[i];
  }
  dst[i] = '\0';
  return out;
}

char* strchr(const char* s, int c) {
  char want = (char)c;
  while (*s) {
    if (*s == want) return (char*)s;
    ++s;
  }
  if (want == '\0') return (char*)s;
  return 0;
}

char* strrchr(const char* s, int c) {
  char want = (char)c;
  const char* last = 0;
  while (*s) {
    if (*s == want) last = s;
    ++s;
  }
  if (want == '\0') return (char*)s;
  return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
  if (!*needle) return (char*)haystack;
  for (const char* h = haystack; *h; ++h) {
    const char* hs = h;
    const char* ns = needle;
    while (*hs && *ns && *hs == *ns) {
      ++hs;
      ++ns;
    }
    if (!*ns) return (char*)h;
  }
  return 0;
}

static int char_in_set(char c, const char* set) {
  while (*set) {
    if (*set == c) return 1;
    ++set;
  }
  return 0;
}

size_t strspn(const char* s, const char* accept) {
  size_t n = 0;
  while (s[n] && char_in_set(s[n], accept)) {
    ++n;
  }
  return n;
}

size_t strcspn(const char* s, const char* reject) {
  size_t n = 0;
  while (s[n] && !char_in_set(s[n], reject)) {
    ++n;
  }
  return n;
}

char* strpbrk(const char* s, const char* accept) {
  while (*s) {
    if (char_in_set(*s, accept)) return (char*)s;
    ++s;
  }
  return 0;
}

char* strtok(char* s, const char* delim) {
  static char* next;
  char* start;
  if (s) {
    next = s;
  } else if (!next) {
    return 0;
  }

  while (*next && char_in_set(*next, delim)) {
    ++next;
  }
  if (!*next) {
    next = 0;
    return 0;
  }

  start = next;
  while (*next && !char_in_set(*next, delim)) {
    ++next;
  }
  if (*next) {
    *next = '\0';
    ++next;
  } else {
    next = 0;
  }
  return start;
}

char* strerror(int errnum) {
  switch (errnum) {
    case 0: return "no error";
    case EINVAL: return "invalid argument";
    case ENOENT: return "no such file or directory";
    case EIO: return "input/output error";
    case ENOMEM: return "not enough memory";
    case EACCES: return "permission denied";
    default: return "unknown error";
  }
}

char* strdup(const char* s) {
  size_t n = strlen(s) + 1;
  char* out = (char*)malloc(n);
  if (!out) return 0;
  memcpy(out, s, n);
  return out;
}

int strcoll(const char* a, const char* b) {
  return strcmp(a, b);
}

size_t strxfrm(char* dst, const char* src, size_t n) {
  size_t len = strlen(src);
  if (n != 0) {
    size_t copy = len;
    if (copy >= n) copy = n - 1;
    memcpy(dst, src, copy);
    dst[copy] = '\0';
  }
  return len;
}
