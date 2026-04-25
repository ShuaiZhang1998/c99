#include <math.h>

static const double PI = 3.14159265358979323846;
static const double TWO_PI = 6.28318530717958647692;
static const double HALF_PI = 1.57079632679489661923;

double fabs(double x) {
  return x < 0.0 ? -x : x;
}

float fabsf(float x) {
  return x < 0.0f ? -x : x;
}

static double sqrt_impl(double x) {
  if (x <= 0.0) return 0.0;
  double guess = x >= 1.0 ? x : 1.0;
  for (int i = 0; i < 32; ++i) {
    guess = 0.5 * (guess + x / guess);
  }
  return guess;
}

double sqrt(double x) {
  return sqrt_impl(x);
}

float sqrtf(float x) {
  return (float)sqrt_impl((double)x);
}

static double pow_impl(double x, double y) {
  long long exp = (long long)y;
  if ((double)exp != y) return 0.0;
  if (exp == 0) return 1.0;
  if (x == 0.0) return exp < 0 ? 0.0 : 0.0;
  int neg = 0;
  if (exp < 0) {
    neg = 1;
    exp = -exp;
  }
  double result = 1.0;
  double base = x;
  while (exp > 0) {
    if (exp & 1LL) result *= base;
    base *= base;
    exp >>= 1;
  }
  return neg ? 1.0 / result : result;
}

double pow(double x, double y) {
  return pow_impl(x, y);
}

float powf(float x, float y) {
  return (float)pow_impl((double)x, (double)y);
}

static double reduce_angle(double x) {
  while (x > PI) x -= TWO_PI;
  while (x < -PI) x += TWO_PI;
  return x;
}

static double sin_poly(double x) {
  double x2 = x * x;
  return x * (1.0 - x2 / 6.0 + (x2 * x2) / 120.0 - (x2 * x2 * x2) / 5040.0);
}

static double cos_poly(double x) {
  double x2 = x * x;
  return 1.0 - x2 / 2.0 + (x2 * x2) / 24.0 - (x2 * x2 * x2) / 720.0;
}

double sin(double x) {
  x = reduce_angle(x);
  if (x > HALF_PI) x = PI - x;
  else if (x < -HALF_PI) x = -PI - x;
  return sin_poly(x);
}

float sinf(float x) {
  return (float)sin((double)x);
}

double cos(double x) {
  x = reduce_angle(x);
  if (x > HALF_PI) return -cos_poly(PI - x);
  if (x < -HALF_PI) return -cos_poly(PI + x);
  return cos_poly(x);
}

float cosf(float x) {
  return (float)cos((double)x);
}
