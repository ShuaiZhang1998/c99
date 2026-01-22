#include <stdarg.h>
#include <stdint.h>
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
  Reader* base;
  int* count;
} CountReader;

static int counted_read(void* ctx) {
  CountReader* cr = (CountReader*)ctx;
  int ch = cr->base->read(cr->base->ctx);
  if (ch >= 0) (*cr->count)++;
  return ch;
}

static void counted_unread(void* ctx, int ch) {
  CountReader* cr = (CountReader*)ctx;
  if (ch >= 0) (*cr->count)--;
  cr->base->unread(cr->base->ctx, ch);
}

typedef struct {
  FILE* f;
} FileReader;

static int file_read_char(void* ctx) {
  FileReader* r = (FileReader*)ctx;
  return fgetc(r->f);
}

static void file_unread_char(void* ctx, int ch) {
  FileReader* r = (FileReader*)ctx;
  if (ch < 0) return;
  ungetc(ch, r->f);
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

static int read_limited(Reader* r, int* width, int* eof);
static void unread_limited(Reader* r, int* width, int ch);

static int scan_string_discard(Reader* r, int width, int* eof) {
  if (!skip_ws(r, eof)) return 0;
  int w = width;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  if (isspace(ch)) {
    unread_limited(r, &w, ch);
    return 0;
  }
  while (1) {
    if (w == 0) break;
    ch = read_limited(r, &w, eof);
    if (ch < 0) break;
    if (isspace(ch)) {
      unread_limited(r, &w, ch);
      break;
    }
  }
  return 1;
}

static int scan_chars(Reader* r, char* out, int width, int* eof) {
  int count = (width > 0) ? width : 1;
  for (int i = 0; i < count; ++i) {
    int ch = r->read(r->ctx);
    if (ch < 0) {
      *eof = 1;
      return 0;
    }
    out[i] = (char)ch;
  }
  return 1;
}

static int scan_chars_discard(Reader* r, int width, int* eof) {
  int count = (width > 0) ? width : 1;
  for (int i = 0; i < count; ++i) {
    int ch = r->read(r->ctx);
    if (ch < 0) {
      *eof = 1;
      return 0;
    }
  }
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

static int scan_int_width(Reader* r, int width, int* out, int* eof) {
  int w = width;
  if (!skip_ws(r, eof)) return 0;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  int sign = 1;
  if (ch == '+' || ch == '-') {
    if (ch == '-') sign = -1;
    ch = read_limited(r, &w, eof);
    if (ch < 0) return 0;
  }
  if (ch < '0' || ch > '9') {
    unread_limited(r, &w, ch);
    return 0;
  }
  unsigned int uv = 0;
  while (ch >= '0' && ch <= '9') {
    uv = uv * 10u + (unsigned int)(ch - '0');
    ch = read_limited(r, &w, eof);
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  *out = (int)(sign * (int)uv);
  return 1;
}

static int scan_long_width(Reader* r, int width, long* out, int* eof) {
  int w = width;
  if (!skip_ws(r, eof)) return 0;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  long sign = 1;
  if (ch == '+' || ch == '-') {
    if (ch == '-') sign = -1;
    ch = read_limited(r, &w, eof);
    if (ch < 0) return 0;
  }
  if (ch < '0' || ch > '9') {
    unread_limited(r, &w, ch);
    return 0;
  }
  unsigned long uv = 0;
  while (ch >= '0' && ch <= '9') {
    uv = uv * 10ul + (unsigned long)(ch - '0');
    ch = read_limited(r, &w, eof);
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  *out = sign * (long)uv;
  return 1;
}

static int scan_ulong_base(Reader* r, unsigned long* out, int base, int width, int* eof) {
  if (!skip_ws(r, eof)) return 0;
  int w = width;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  unsigned long v = 0;
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
    v = v * (unsigned long)base + (unsigned long)digit;
    any = 1;
    ch = read_limited(r, &w, eof);
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  if (!any) return 0;
  *out = v;
  return 1;
}

static int scan_longlong_width(Reader* r, int width, long long* out, int* eof) {
  int w = width;
  if (!skip_ws(r, eof)) return 0;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  long long sign = 1;
  if (ch == '+' || ch == '-') {
    if (ch == '-') sign = -1;
    ch = read_limited(r, &w, eof);
    if (ch < 0) return 0;
  }
  if (ch < '0' || ch > '9') {
    unread_limited(r, &w, ch);
    return 0;
  }
  unsigned long long uv = 0;
  while (ch >= '0' && ch <= '9') {
    uv = uv * 10ull + (unsigned long long)(ch - '0');
    ch = read_limited(r, &w, eof);
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  *out = sign * (long long)uv;
  return 1;
}

static int scan_ulonglong_base(
    Reader* r, unsigned long long* out, int base, int width, int* eof) {
  if (!skip_ws(r, eof)) return 0;
  int w = width;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  unsigned long long v = 0;
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
    v = v * (unsigned long long)base + (unsigned long long)digit;
    any = 1;
    ch = read_limited(r, &w, eof);
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  if (!any) return 0;
  *out = v;
  return 1;
}

static int scan_int_auto_width(Reader* r, int width, int* out, int* eof) {
  int w = width;
  if (!skip_ws(r, eof)) return 0;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  int sign = 1;
  if (ch == '+' || ch == '-') {
    if (ch == '-') sign = -1;
    ch = read_limited(r, &w, eof);
    if (ch < 0) return 0;
  }
  int base = 10;
  if (ch == '0') {
    int next = read_limited(r, &w, eof);
    if (next == 'x' || next == 'X') {
      base = 16;
      ch = read_limited(r, &w, eof);
    } else {
      base = 8;
      unread_limited(r, &w, next);
      ch = '0';
    }
  }
  if (!((ch >= '0' && ch <= '9') || (base == 16 && ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))))) {
    unread_limited(r, &w, ch);
    return 0;
  }
  unsigned int uv = 0;
  while (ch >= 0) {
    int digit = -1;
    if (ch >= '0' && ch <= '9') digit = ch - '0';
    else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
    if (digit < 0 || digit >= base) break;
    uv = uv * (unsigned int)base + (unsigned int)digit;
    ch = read_limited(r, &w, eof);
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  *out = (int)(sign * (int)uv);
  return 1;
}

static int scan_long_auto_width(Reader* r, int width, long* out, int* eof) {
  int w = width;
  if (!skip_ws(r, eof)) return 0;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  long sign = 1;
  if (ch == '+' || ch == '-') {
    if (ch == '-') sign = -1;
    ch = read_limited(r, &w, eof);
    if (ch < 0) return 0;
  }
  int base = 10;
  if (ch == '0') {
    int next = read_limited(r, &w, eof);
    if (next == 'x' || next == 'X') {
      base = 16;
      ch = read_limited(r, &w, eof);
    } else {
      base = 8;
      unread_limited(r, &w, next);
      ch = '0';
    }
  }
  if (!((ch >= '0' && ch <= '9') || (base == 16 && ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))))) {
    unread_limited(r, &w, ch);
    return 0;
  }
  unsigned long uv = 0;
  while (ch >= 0) {
    int digit = -1;
    if (ch >= '0' && ch <= '9') digit = ch - '0';
    else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
    if (digit < 0 || digit >= base) break;
    uv = uv * (unsigned long)base + (unsigned long)digit;
    ch = read_limited(r, &w, eof);
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  *out = sign * (long)uv;
  return 1;
}

static int scan_longlong_auto_width(Reader* r, int width, long long* out, int* eof) {
  int w = width;
  if (!skip_ws(r, eof)) return 0;
  int ch = read_limited(r, &w, eof);
  if (ch < 0) return 0;
  long long sign = 1;
  if (ch == '+' || ch == '-') {
    if (ch == '-') sign = -1;
    ch = read_limited(r, &w, eof);
    if (ch < 0) return 0;
  }
  int base = 10;
  if (ch == '0') {
    int next = read_limited(r, &w, eof);
    if (next == 'x' || next == 'X') {
      base = 16;
      ch = read_limited(r, &w, eof);
    } else {
      base = 8;
      unread_limited(r, &w, next);
      ch = '0';
    }
  }
  if (!((ch >= '0' && ch <= '9') ||
        (base == 16 && ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))))) {
    unread_limited(r, &w, ch);
    return 0;
  }
  unsigned long long uv = 0;
  while (ch >= 0) {
    int digit = -1;
    if (ch >= '0' && ch <= '9') digit = ch - '0';
    else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
    if (digit < 0 || digit >= base) break;
    uv = uv * (unsigned long long)base + (unsigned long long)digit;
    ch = read_limited(r, &w, eof);
  }
  if (ch >= 0 && ch != -2) unread_limited(r, &w, ch);
  *out = sign * (long long)uv;
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

static int scan_impl(Reader* base, const char* fmt, va_list ap) {
  int count = 0;
  CountReader cr = {base, &count};
  Reader wrapped;
  wrapped.read = counted_read;
  wrapped.unread = counted_unread;
  wrapped.ctx = &cr;
  Reader* r = &wrapped;
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
    int suppress = 0;
    if (*p == '*') {
      suppress = 1;
      ++p;
    }
    int width = -1;
    if (*p >= '0' && *p <= '9') {
      width = 0;
      while (*p >= '0' && *p <= '9') {
        width = width * 10 + (*p - '0');
        ++p;
      }
    }
    char len = 0;
    if (*p == 'l') {
      if (*(p + 1) == 'l') {
        len = 'L';
        p += 2;
      } else {
        len = 'l';
        ++p;
      }
    } else if (*p == 'h') {
      len = 'h';
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
      if (suppress) {
        long long tmp = 0;
        if (len == 'L') {
          if (!scan_longlong_width(r, width, &tmp, &eof)) break;
        } else if (len == 'l') {
          long ltmp = 0;
          if (width < 0) {
            int itmp = 0;
            if (!scan_int(r, &itmp, &eof)) break;
            ltmp = (long)itmp;
          } else if (!scan_long_width(r, width, &ltmp, &eof)) break;
        } else {
          int itmp = 0;
          if (width < 0) {
            if (!scan_int(r, &itmp, &eof)) break;
          } else if (!scan_int_width(r, width, &itmp, &eof)) break;
        }
      } else {
        if (len == 'L') {
          long long* out = va_arg(ap, long long*);
          if (!out) return assigned;
          if (!scan_longlong_width(r, width, out, &eof)) break;
        } else if (len == 'l') {
          long* out = va_arg(ap, long*);
          if (!out) return assigned;
          if (width < 0) {
            int tmp = 0;
            if (!scan_int(r, &tmp, &eof)) break;
            *out = (long)tmp;
          } else {
            if (!scan_long_width(r, width, out, &eof)) break;
          }
        } else {
          int* out = va_arg(ap, int*);
          if (!out) return assigned;
          if (width < 0) {
            if (!scan_int(r, out, &eof)) break;
          } else {
            if (!scan_int_width(r, width, out, &eof)) break;
          }
        }
        assigned++;
      }
      continue;
    }
    if (*p == 'u') {
      if (suppress) {
        unsigned long long tmp = 0;
        if (len == 'L') {
          if (!scan_ulonglong_base(r, &tmp, 10, width, &eof)) break;
        } else if (len == 'l') {
          unsigned long ltmp = 0;
          if (!scan_ulong_base(r, &ltmp, 10, width, &eof)) break;
        } else {
          unsigned int itmp = 0;
          if (!scan_uint_base(r, &itmp, 10, width, &eof)) break;
        }
      } else {
        if (len == 'L') {
          unsigned long long* out = va_arg(ap, unsigned long long*);
          if (!out) return assigned;
          if (!scan_ulonglong_base(r, out, 10, width, &eof)) break;
        } else if (len == 'l') {
          unsigned long* out = va_arg(ap, unsigned long*);
          if (!out) return assigned;
          if (!scan_ulong_base(r, out, 10, width, &eof)) break;
        } else {
          unsigned int* out = va_arg(ap, unsigned int*);
          if (!out) return assigned;
          if (!scan_uint_base(r, out, 10, width, &eof)) break;
        }
        assigned++;
      }
      continue;
    }
    if (*p == 'o') {
      if (suppress) {
        unsigned long long tmp = 0;
        if (len == 'L') {
          if (!scan_ulonglong_base(r, &tmp, 8, width, &eof)) break;
        } else if (len == 'l') {
          unsigned long ltmp = 0;
          if (!scan_ulong_base(r, &ltmp, 8, width, &eof)) break;
        } else {
          unsigned int itmp = 0;
          if (!scan_uint_base(r, &itmp, 8, width, &eof)) break;
        }
      } else {
        if (len == 'L') {
          unsigned long long* out = va_arg(ap, unsigned long long*);
          if (!out) return assigned;
          if (!scan_ulonglong_base(r, out, 8, width, &eof)) break;
        } else if (len == 'l') {
          unsigned long* out = va_arg(ap, unsigned long*);
          if (!out) return assigned;
          if (!scan_ulong_base(r, out, 8, width, &eof)) break;
        } else {
          unsigned int* out = va_arg(ap, unsigned int*);
          if (!out) return assigned;
          if (!scan_uint_base(r, out, 8, width, &eof)) break;
        }
        assigned++;
      }
      continue;
    }
    if (*p == 'i') {
      if (suppress) {
        long long tmp = 0;
        if (len == 'L') {
          if (!scan_longlong_auto_width(r, width, &tmp, &eof)) break;
        } else if (len == 'l') {
          long ltmp = 0;
          if (!scan_long_auto_width(r, width, &ltmp, &eof)) break;
        } else {
          int itmp = 0;
          if (!scan_int_auto_width(r, width, &itmp, &eof)) break;
        }
      } else {
        if (len == 'L') {
          long long* out = va_arg(ap, long long*);
          if (!out) return assigned;
          if (!scan_longlong_auto_width(r, width, out, &eof)) break;
        } else if (len == 'l') {
          long* out = va_arg(ap, long*);
          if (!out) return assigned;
          if (!scan_long_auto_width(r, width, out, &eof)) break;
        } else {
          int* out = va_arg(ap, int*);
          if (!out) return assigned;
          if (!scan_int_auto_width(r, width, out, &eof)) break;
        }
        assigned++;
      }
      continue;
    }
    if (*p == 'x') {
      if (suppress) {
        unsigned long long tmp = 0;
        if (len == 'L') {
          if (!scan_ulonglong_base(r, &tmp, 16, width, &eof)) break;
        } else if (len == 'l') {
          unsigned long ltmp = 0;
          if (!scan_ulong_base(r, &ltmp, 16, width, &eof)) break;
        } else {
          unsigned int itmp = 0;
          if (!scan_uint_base(r, &itmp, 16, width, &eof)) break;
        }
      } else {
        if (len == 'L') {
          unsigned long long* out = va_arg(ap, unsigned long long*);
          if (!out) return assigned;
          if (!scan_ulonglong_base(r, out, 16, width, &eof)) break;
        } else if (len == 'l') {
          unsigned long* out = va_arg(ap, unsigned long*);
          if (!out) return assigned;
          if (!scan_ulong_base(r, out, 16, width, &eof)) break;
        } else {
          unsigned int* out = va_arg(ap, unsigned int*);
          if (!out) return assigned;
          if (!scan_uint_base(r, out, 16, width, &eof)) break;
        }
        assigned++;
      }
      continue;
    }
    if (*p == 'p') {
      if (suppress) {
        unsigned long long tmp = 0;
        if (!scan_ulonglong_base(r, &tmp, 16, width, &eof)) break;
      } else {
        void** out = va_arg(ap, void**);
        if (!out) return assigned;
        unsigned long long tmp = 0;
        if (!scan_ulonglong_base(r, &tmp, 16, width, &eof)) break;
        *out = (void*)(uintptr_t)tmp;
        assigned++;
      }
      continue;
    }
    if (*p == 'f' || *p == 'e' || *p == 'E' || *p == 'g' || *p == 'G') {
      double v = 0.0;
      if (!scan_float(r, &v, width, &eof)) break;
      if (suppress) {
        continue;
      }
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
      if (suppress) {
        if (width >= 0) {
          if (!scan_string_discard(r, width, &eof)) break;
        } else {
          if (!scan_string_discard(r, 0x7fffffff, &eof)) break;
        }
      } else {
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
      }
      continue;
    }
    if (*p == 'c') {
      if (suppress) {
        if (!scan_chars_discard(r, width, &eof)) break;
      } else {
        char* out = va_arg(ap, char*);
        if (!out) return assigned;
        if (width > 1) {
          if (!scan_chars(r, out, width, &eof)) break;
        } else {
          if (!scan_char(r, out, &eof)) break;
        }
        assigned++;
      }
      continue;
    }
    if (*p == 'n') {
      if (!suppress) {
        if (len == 'L') {
          long long* out = va_arg(ap, long long*);
          if (!out) return assigned;
          *out = (long long)count;
        } else if (len == 'l') {
          long* out = va_arg(ap, long*);
          if (!out) return assigned;
          *out = (long)count;
        } else {
          int* out = va_arg(ap, int*);
          if (!out) return assigned;
          *out = count;
        }
      }
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

int fscanf(FILE* f, const char* fmt, ...) {
  if (!f || !fmt) return -1;
  FileReader fr;
  fr.f = f;
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
