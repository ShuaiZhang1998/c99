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

static int write_uint(unsigned long long v) {
  char buf[32];
  int i = 0;
  do {
    buf[i++] = (char)('0' + (v % 10));
    v /= 10;
  } while (v > 0);
  for (int j = i - 1; j >= 0; --j) {
    write_char(buf[j]);
  }
  return i;
}

static int write_spaces(int n) {
  for (int i = 0; i < n; ++i) {
    write_char(' ');
  }
  return n;
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

static int write_float(double v, int precision) {
  int count = 0;
  if (v < 0) {
    write_char('-');
    count++;
    v = -v;
  }
  v = apply_rounding(v, precision);
  unsigned long long ip = (unsigned long long)v;
  double frac = v - (double)ip;
  count += write_uint(ip);
  if (precision != 0) {
    write_char('.');
    count++;
  }
  int prec = (precision < 0) ? 6 : precision;
  for (int i = 0; i < prec; ++i) {
    frac *= 10.0;
    int digit = (int)frac;
    write_char((char)('0' + digit));
    count++;
    frac -= digit;
  }
  return count;
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
      if (width > len) count += write_spaces(width - len);
      write_char('%');
      count += len;
      continue;
    }
    if (*p == 'd' || *p == 'i') {
      int v = va_arg(ap, int);
      int len = count_int(v);
      if (width > len) count += write_spaces(width - len);
      count += write_int(v);
      continue;
    }
    if (*p == 'c') {
      int c = va_arg(ap, int);
      if (width > 1) count += write_spaces(width - 1);
      write_char((char)c);
      count++;
      continue;
    }
    if (*p == 'f') {
      double v = va_arg(ap, double);
      int len = float_len(v, precision);
      if (width > len) count += write_spaces(width - len);
      count += write_float(v, precision);
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
      if (width > len) count += write_spaces(width - len);
      for (int i = 0; i < len; ++i) {
        write_char(t[i]);
      }
      count += len;
      continue;
    }
    write_char('%');
    write_char(*p);
    count += 2;
  }
  va_end(ap);
  return count;
}

int putchar(int c) {
  return write_char((char)c);
}

int puts(const char* s) {
  int count = write_str(s);
  write_char('\n');
  return count + 1;
}
