#include <stdarg.h>
#include "ctype.h"
#include "stdio.h"

extern void c99cc_init_stdio(void);
extern int c99cc_read_file(FILE* f, unsigned char* buf, size_t len);

typedef struct {
  int (*read)(void* ctx);
  void (*unread)(void* ctx, int ch);
  void* ctx;
} Reader;

typedef struct {
  FILE* f;
  int has_peek;
  int peek;
} FileReader;

static int file_read_char(void* ctx) {
  FileReader* r = (FileReader*)ctx;
  if (r->has_peek) {
    r->has_peek = 0;
    return r->peek;
  }
  unsigned char ch = 0;
  int n = c99cc_read_file(r->f, &ch, 1);
  if (n != 1) return -1;
  return (int)ch;
}

static void file_unread_char(void* ctx, int ch) {
  FileReader* r = (FileReader*)ctx;
  if (ch < 0) return;
  r->has_peek = 1;
  r->peek = ch;
}

typedef struct {
  const char* s;
  size_t pos;
  int has_peek;
  int peek;
} StrReader;

static int str_read_char(void* ctx) {
  StrReader* r = (StrReader*)ctx;
  if (r->has_peek) {
    r->has_peek = 0;
    return r->peek;
  }
  unsigned char ch = (unsigned char)r->s[r->pos];
  if (ch == '\0') return -1;
  r->pos++;
  return (int)ch;
}

static void str_unread_char(void* ctx, int ch) {
  StrReader* r = (StrReader*)ctx;
  if (ch < 0) return;
  r->has_peek = 1;
  r->peek = ch;
}

static int skip_ws(Reader* r, int* eof) {
  int ch;
  do {
    ch = r->read(r->ctx);
    if (ch < 0) {
      *eof = 1;
      return 0;
    }
  } while (isspace(ch));
  r->unread(r->ctx, ch);
  return 1;
}

static int scan_int(Reader* r, int* out, int* eof) {
  if (!skip_ws(r, eof)) return 0;
  int ch = r->read(r->ctx);
  if (ch < 0) {
    *eof = 1;
    return 0;
  }
  int sign = 1;
  if (ch == '+' || ch == '-') {
    if (ch == '-') sign = -1;
    ch = r->read(r->ctx);
    if (ch < 0) {
      *eof = 1;
      return 0;
    }
  }
  if (!isdigit(ch)) {
    r->unread(r->ctx, ch);
    return 0;
  }
  int v = 0;
  while (isdigit(ch)) {
    v = v * 10 + (ch - '0');
    ch = r->read(r->ctx);
    if (ch < 0) {
      *out = sign * v;
      return 1;
    }
  }
  r->unread(r->ctx, ch);
  *out = sign * v;
  return 1;
}

static int scan_string(Reader* r, char* out, int* eof) {
  if (!skip_ws(r, eof)) return 0;
  int ch = r->read(r->ctx);
  if (ch < 0) {
    *eof = 1;
    return 0;
  }
  if (isspace(ch)) {
    r->unread(r->ctx, ch);
    return 0;
  }
  char* dst = out;
  *dst++ = (char)ch;
  while (1) {
    ch = r->read(r->ctx);
    if (ch < 0) break;
    if (isspace(ch)) {
      r->unread(r->ctx, ch);
      break;
    }
    *dst++ = (char)ch;
  }
  *dst = '\0';
  return 1;
}

static int scan_char(Reader* r, char* out, int* eof) {
  int ch = r->read(r->ctx);
  if (ch < 0) {
    *eof = 1;
    return 0;
  }
  *out = (char)ch;
  return 1;
}

static int read_limited(Reader* r, int* width, int* eof) {
  if (width && *width == 0) return -2;
  int ch = r->read(r->ctx);
  if (ch < 0) {
    *eof = 1;
    return -1;
  }
  if (width && *width > 0) (*width)--;
  return ch;
}

static void unread_limited(Reader* r, int* width, int ch) {
  if (ch < 0) return;
  if (width && *width >= 0) (*width)++;
  r->unread(r->ctx, ch);
}

static int scan_uint_base(Reader* r, unsigned int* out, int base, int width, int* eof) {
  if (!skip_ws(r, eof)) return 0;
  int w = width;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  unsigned int v = 0;
  int any = 0;
  if (base == 16 && ch == '0') {
    int next = read_limited(r, &w, eof);
    if (next == 'x' || next == 'X') {
      ch = read_limited(r, &w, eof);
    } else {
      unread_limited(r, &w, next);
    }
  }
  while (ch >= 0) {
    int digit = -1;
    if (ch >= '0' && ch <= '9') digit = ch - '0';
    else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
    if (digit < 0 || digit >= base) break;
    v = v * (unsigned int)base + (unsigned int)digit;
    any = 1;
    ch = read_limited(r, &w, eof);
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  if (!any) return 0;
  *out = v;
  return 1;
}

static double pow10_int(int exp) {
  double v = 1.0;
  int e = exp < 0 ? -exp : exp;
  while (e--) v *= 10.0;
  if (exp < 0) return 1.0 / v;
  return v;
}

static int scan_float(Reader* r, double* out, int width, int* eof) {
  if (!skip_ws(r, eof)) return 0;
  int w = width;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  int sign = 1;
  if (ch == '+' || ch == '-') {
    if (ch == '-') sign = -1;
    ch = read_limited(r, &w, eof);
    if (ch < 0) return 0;
  }
  int any = 0;
  double int_part = 0.0;
  while (ch >= 0 && ch >= '0' && ch <= '9') {
    int_part = int_part * 10.0 + (double)(ch - '0');
    any = 1;
    ch = read_limited(r, &w, eof);
  }
  double frac_part = 0.0;
  int frac_len = 0;
  if (ch == '.') {
    ch = read_limited(r, &w, eof);
    while (ch >= 0 && ch >= '0' && ch <= '9') {
      frac_part = frac_part * 10.0 + (double)(ch - '0');
      frac_len++;
      any = 1;
      ch = read_limited(r, &w, eof);
    }
  }
  if (!any) {
    if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
    return 0;
  }
  int exp_val = 0;
  int exp_sign = 1;
  if (ch == 'e' || ch == 'E') {
    int ch2 = read_limited(r, &w, eof);
    if (ch2 == '+' || ch2 == '-') {
      if (ch2 == '-') exp_sign = -1;
      ch2 = read_limited(r, &w, eof);
    }
    if (ch2 >= '0' && ch2 <= '9') {
      while (ch2 >= '0' && ch2 <= '9') {
        exp_val = exp_val * 10 + (ch2 - '0');
        ch2 = read_limited(r, &w, eof);
      }
      if (ch2 >= 0 && ch2 != -2) unread_limited(r, &w, ch2);
    } else {
      unread_limited(r, &w, ch2);
      unread_limited(r, &w, ch);
      exp_val = 0;
      exp_sign = 1;
      ch = read_limited(r, &w, eof);
      if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
    }
    ch = -2;
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  double v = int_part;
  if (frac_len > 0) v += frac_part / pow10_int(frac_len);
  if (exp_val != 0) v *= pow10_int(exp_sign * exp_val);
  *out = (double)sign * v;
  return 1;
}

static int scan_impl(Reader* r, const char* fmt, va_list ap) {
  int assigned = 0;
  int eof = 0;
  for (const char* p = fmt; *p; ++p) {
    if (isspace((unsigned char)*p)) {
      while (isspace((unsigned char)*p)) ++p;
      --p;
      if (!skip_ws(r, &eof)) break;
      continue;
    }
    if (*p != '%') {
      int ch = r->read(r->ctx);
      if (ch < 0) {
        eof = 1;
        break;
      }
      if (ch != (unsigned char)*p) {
        r->unread(r->ctx, ch);
        break;
      }
      continue;
    }
    ++p;
    if (*p == '\0') break;
    int width = -1;
    if (*p >= '0' && *p <= '9') {
      width = 0;
      while (*p >= '0' && *p <= '9') {
        width = width * 10 + (*p - '0');
        ++p;
      }
    }
    char len = 0;
    if (*p == 'l' || *p == 'h') {
      len = *p;
      ++p;
    }
    if (*p == '%') {
      int ch = r->read(r->ctx);
      if (ch < 0) {
        eof = 1;
        break;
      }
      if (ch != '%') {
        r->unread(r->ctx, ch);
        break;
      }
      continue;
    }
    if (*p == 'd') {
      int* out = va_arg(ap, int*);
      if (!out) return assigned;
      int w = width;
      if (w < 0) {
        if (!scan_int(r, out, &eof)) break;
      } else {
        unsigned int uv = 0;
        int sign = 1;
        if (!skip_ws(r, &eof)) break;
        int ch = read_limited(r, &w, &eof);
        if (ch < 0) break;
        if (ch == '+' || ch == '-') {
          if (ch == '-') sign = -1;
          ch = read_limited(r, &w, &eof);
          if (ch < 0) break;
        }
        if (ch < '0' || ch > '9') {
          unread_limited(r, &w, ch);
          break;
        }
        while (ch >= '0' && ch <= '9') {
          uv = uv * 10u + (unsigned int)(ch - '0');
          ch = read_limited(r, &w, &eof);
        }
        if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
        *out = (int)(sign * (int)uv);
      }
      assigned++;
      continue;
    }
    if (*p == 'u') {
      unsigned int* out = va_arg(ap, unsigned int*);
      if (!out) return assigned;
      if (!scan_uint_base(r, out, 10, width, &eof)) break;
      assigned++;
      continue;
    }
    if (*p == 'x') {
      unsigned int* out = va_arg(ap, unsigned int*);
      if (!out) return assigned;
      if (!scan_uint_base(r, out, 16, width, &eof)) break;
      assigned++;
      continue;
    }
    if (*p == 'f') {
      double v = 0.0;
      if (!scan_float(r, &v, width, &eof)) break;
      if (len == 'l') {
        double* out = va_arg(ap, double*);
        if (!out) return assigned;
        *out = v;
      } else {
        float* out = va_arg(ap, float*);
        if (!out) return assigned;
        *out = (float)v;
      }
      assigned++;
      continue;
    }
    if (*p == 's') {
      char* out = va_arg(ap, char*);
      if (!out) return assigned;
      if (width >= 0) {
        if (!skip_ws(r, &eof)) break;
        int w = width;
        int ch = read_limited(r, &w, &eof);
        if (ch < 0) break;
        if (ch == -2) break;
        if (isspace(ch)) {
          unread_limited(r, &w, ch);
          break;
        }
        char* dst = out;
        *dst++ = (char)ch;
        while (w != 0) {
          ch = read_limited(r, &w, &eof);
          if (ch < 0) break;
          if (ch == -2) break;
          if (isspace(ch)) {
            unread_limited(r, &w, ch);
            break;
          }
          *dst++ = (char)ch;
        }
        *dst = '\0';
      } else {
        if (!scan_string(r, out, &eof)) break;
      }
      assigned++;
      continue;
    }
    if (*p == 'c') {
      char* out = va_arg(ap, char*);
      if (!out) return assigned;
      if (!scan_char(r, out, &eof)) break;
      assigned++;
      continue;
    }
    break;
  }
  if (assigned == 0 && eof) return -1;
  return assigned;
}

int scanf(const char* fmt, ...) {
  if (!fmt) return -1;
  c99cc_init_stdio();
  FileReader fr;
  fr.f = stdin;
  fr.has_peek = 0;
  fr.peek = 0;
  Reader r;
  r.read = file_read_char;
  r.unread = file_unread_char;
  r.ctx = &fr;
  va_list ap;
  va_start(ap, fmt);
  int n = scan_impl(&r, fmt, ap);
  va_end(ap);
  return n;
}

int sscanf(const char* s, const char* fmt, ...) {
  if (!s || !fmt) return -1;
  StrReader sr;
  sr.s = s;
  sr.pos = 0;
  sr.has_peek = 0;
  sr.peek = 0;
  Reader r;
  r.read = str_read_char;
  r.unread = str_unread_char;
  r.ctx = &sr;
  va_list ap;
  va_start(ap, fmt);
  int n = scan_impl(&r, fmt, ap);
  va_end(ap);
  return n;
}
