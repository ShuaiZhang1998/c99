#include <stdarg.h>
#include <unistd.h>

static int write_char(char c) {
  return (int)write(1, &c, 1);
}

static int write_str(const char* s) {
  int n = 0;
  if (!s) s = "(null)";
  while (*s) {
    write_char(*s++);
    n++;
  }
  return n;
}

static int write_int(int v) {
  char buf[32];
  int i = 0;
  unsigned int u = (v < 0) ? (unsigned int)(-v) : (unsigned int)v;
  if (v < 0) {
    write_char('-');
  }
  do {
    buf[i++] = (char)('0' + (u % 10));
    u /= 10;
  } while (u > 0);
  for (int j = i - 1; j >= 0; --j) {
    write_char(buf[j]);
  }
  return i + (v < 0 ? 1 : 0);
}

int printf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int count = 0;
  for (const char* p = fmt; *p; ++p) {
    if (*p != '%') {
      write_char(*p);
      count++;
      continue;
    }
    ++p;
    if (*p == '\0') break;
    if (*p == '%') {
      write_char('%');
      count++;
      continue;
    }
    if (*p == 'd' || *p == 'i') {
      int v = va_arg(ap, int);
      count += write_int(v);
      continue;
    }
    if (*p == 'c') {
      int c = va_arg(ap, int);
      write_char((char)c);
      count++;
      continue;
    }
    if (*p == 's') {
      const char* s = va_arg(ap, const char*);
      count += write_str(s);
      continue;
    }
    write_char('%');
    write_char(*p);
    count += 2;
  }
  va_end(ap);
  return count;
}
