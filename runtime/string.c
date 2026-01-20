#include <stddef.h>

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
