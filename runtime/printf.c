#include <stdarg.h>
#include <stddef.h>
#include "stdio.h"

extern void c99cc_init_stdio(void);
extern int c99cc_write_file(FILE* f, const unsigned char* buf, size_t len);

#ifdef snprintf
#undef snprintf
#endif
#ifdef sprintf
#undef sprintf
#endif

typedef int (*WriteFn)(void* ctx, const unsigned char* buf, int len);

typedef struct {
  WriteFn write;
  void* ctx;
  int count;
} Out;

static void out_write(Out* out, const unsigned char* buf, int len) {
  if (len <= 0) return;
  int n = out->write(out->ctx, buf, len);
  if (n > 0) out->count += n;
}

static void out_char(Out* out, char c) {
  out_write(out, (const unsigned char*)&c, 1);
}

static int write_str(Out* out, const char* s) {
  int n = 0;
  if (!s) s = "(null)";
  while (*s) {
    out_char(out, *s++);
    n++;
  }
  return n;
}

static int write_int(Out* out, int v) {
  char buf[32];
  int i = 0;
  unsigned int u = (v < 0) ? (unsigned int)(-v) : (unsigned int)v;
  if (v < 0) {
    out_char(out, '-');
  }
  do {
    buf[i++] = (char)('0' + (u % 10));
    u /= 10;
  } while (u > 0);
  for (int j = i - 1; j >= 0; --j) {
    out_char(out, buf[j]);
  }
  return i + (v < 0 ? 1 : 0);
}

static int count_uint(unsigned long long v) {
  int n = 1;
  while (v >= 10) {
    v /= 10;
    n++;
  }
  return n;
}

static int count_int(int v) {
  unsigned int u = (v < 0) ? (unsigned int)(-v) : (unsigned int)v;
  int n = count_uint(u);
  return n + (v < 0 ? 1 : 0);
}

static int write_uint(Out* out, unsigned long long v) {
  char buf[32];
  int i = 0;
  do {
    buf[i++] = (char)('0' + (v % 10));
    v /= 10;
  } while (v > 0);
  for (int j = i - 1; j >= 0; --j) {
    out_char(out, buf[j]);
  }
  return i;
}

static void write_spaces(Out* out, int n) {
  for (int i = 0; i < n; ++i) {
    out_char(out, ' ');
  }
}

static double apply_rounding(double v, int precision) {
  int prec = (precision < 0) ? 6 : precision;
  if (prec == 0) return v + 0.5;
  double rounder = 0.5;
  for (int i = 0; i < prec; ++i) {
    rounder /= 10.0;
  }
  return v + rounder;
}

static int float_len(double v, int precision) {
  int len = 0;
  if (v < 0) {
    len++;
    v = -v;
  }
  v = apply_rounding(v, precision);
  unsigned long long ip = (unsigned long long)v;
  len += count_uint(ip);
  if (precision > 0 || precision < 0) {
    len += 1 + (precision < 0 ? 6 : precision);
  }
  return len;
}

static int write_float(Out* out, double v, int precision) {
  int count = 0;
  if (v < 0) {
    out_char(out, '-');
    count++;
    v = -v;
  }
  v = apply_rounding(v, precision);
  unsigned long long ip = (unsigned long long)v;
  double frac = v - (double)ip;
  count += write_uint(out, ip);
  if (precision != 0) {
    out_char(out, '.');
    count++;
  }
  int prec = (precision < 0) ? 6 : precision;
  for (int i = 0; i < prec; ++i) {
    frac *= 10.0;
    int digit = (int)frac;
    out_char(out, (char)('0' + digit));
    count++;
    frac -= digit;
  }
  return count;
}

static int format_to_out(Out* out, const char* fmt, va_list ap) {
  for (const char* p = fmt; *p; ++p) {
    if (*p != '%') {
      out_char(out, *p);
      continue;
    }
    ++p;
    if (*p == '\0') break;
    int width = 0;
    int precision = -1;
    while (*p >= '0' && *p <= '9') {
      width = width * 10 + (*p - '0');
      ++p;
    }
    if (*p == '.') {
      ++p;
      precision = 0;
      while (*p >= '0' && *p <= '9') {
        precision = precision * 10 + (*p - '0');
        ++p;
      }
    }
    if (*p == '%') {
      int len = 1;
      if (width > len) write_spaces(out, width - len);
      out_char(out, '%');
      continue;
    }
    if (*p == 'd' || *p == 'i') {
      int v = va_arg(ap, int);
      int len = count_int(v);
      if (width > len) write_spaces(out, width - len);
      write_int(out, v);
      continue;
    }
    if (*p == 'c') {
      int c = va_arg(ap, int);
      if (width > 1) write_spaces(out, width - 1);
      out_char(out, (char)c);
      continue;
    }
    if (*p == 'f') {
      double v = va_arg(ap, double);
      int len = float_len(v, precision);
      if (width > len) write_spaces(out, width - len);
      write_float(out, v, precision);
      continue;
    }
    if (*p == 's') {
      const char* s = va_arg(ap, const char*);
      const char* t = s ? s : "(null)";
      int len = 0;
      while (t[len]) len++;
      if (precision >= 0 && precision < len) {
        len = precision;
      }
      if (width > len) write_spaces(out, width - len);
      for (int i = 0; i < len; ++i) {
        out_char(out, t[i]);
      }
      continue;
    }
    out_char(out, '%');
    out_char(out, *p);
  }
  return out->count;
}

typedef struct {
  char* buf;
  size_t cap;
  size_t pos;
} BufCtx;

static int buf_write(void* ctx, const unsigned char* buf, int len) {
  BufCtx* b = (BufCtx*)ctx;
  size_t limit = b->cap > 0 ? b->cap - 1 : 0;
  if (limit > b->pos) {
    size_t avail = limit - b->pos;
    size_t tocopy = (size_t)len < avail ? (size_t)len : avail;
    for (size_t i = 0; i < tocopy; ++i) {
      b->buf[b->pos + i] = (char)buf[i];
    }
    b->pos += tocopy;
  }
  return len;
}

typedef struct {
  FILE* f;
} FileCtx;

static int file_write(void* ctx, const unsigned char* buf, int len) {
  FileCtx* fc = (FileCtx*)ctx;
  return c99cc_write_file(fc->f, buf, (size_t)len);
}

int printf(const char* fmt, ...) {
  c99cc_init_stdio();
  FileCtx fc;
  fc.f = stdout;
  Out out;
  out.write = file_write;
  out.ctx = &fc;
  out.count = 0;
  va_list ap;
  va_start(ap, fmt);
  int n = format_to_out(&out, fmt, ap);
  va_end(ap);
  return n;
}

int fprintf(FILE* f, const char* fmt, ...) {
  c99cc_init_stdio();
  if (!f) return -1;
  FileCtx fc;
  fc.f = f;
  Out out;
  out.write = file_write;
  out.ctx = &fc;
  out.count = 0;
  va_list ap;
  va_start(ap, fmt);
  int n = format_to_out(&out, fmt, ap);
  va_end(ap);
  return n;
}

int snprintf(char* s, size_t n, const char* fmt, ...) {
  if (!s && n > 0) return -1;
  BufCtx bc;
  bc.buf = s;
  bc.cap = n;
  bc.pos = 0;
  Out out;
  out.write = buf_write;
  out.ctx = &bc;
  out.count = 0;
  va_list ap;
  va_start(ap, fmt);
  int count = format_to_out(&out, fmt, ap);
  va_end(ap);
  if (n > 0) {
    size_t pos = bc.pos < (n - 1) ? bc.pos : (n - 1);
    s[pos] = '\0';
  }
  return count;
}

int sprintf(char* s, const char* fmt, ...) {
  if (!s) return -1;
  BufCtx bc;
  bc.buf = s;
  bc.cap = (size_t)-1;
  bc.pos = 0;
  Out out;
  out.write = buf_write;
  out.ctx = &bc;
  out.count = 0;
  va_list ap;
  va_start(ap, fmt);
  int count = format_to_out(&out, fmt, ap);
  va_end(ap);
  s[bc.pos] = '\0';
  return count;
}

int putchar(int c) {
  c99cc_init_stdio();
  unsigned char ch = (unsigned char)c;
  int n = c99cc_write_file(stdout, &ch, 1);
  return n == 1 ? 1 : -1;
}

int puts(const char* s) {
  c99cc_init_stdio();
  const char* t = s ? s : "(null)";
  int count = 0;
  while (*t) {
    unsigned char ch = (unsigned char)*t++;
    if (c99cc_write_file(stdout, &ch, 1) != 1) return -1;
    count++;
  }
  unsigned char nl = '\n';
  if (c99cc_write_file(stdout, &nl, 1) != 1) return -1;
  return count + 1;
}
