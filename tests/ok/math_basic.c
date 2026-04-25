// ARGS: -I include
// EXPECT: 0
#include <math.h>

int main() {
  double a = fabs(-3.5);
  if (a < 3.49 || a > 3.51) return 1;

  float b = fabsf(-2.25f);
  if (b < 2.24f || b > 2.26f) return 2;

  double c = sqrt(9.0);
  if (c < 2.99 || c > 3.01) return 3;

  float d = sqrtf(16.0f);
  if (d < 3.99f || d > 4.01f) return 4;

  if (sqrt(0.0) != 0.0) return 5;
  if (sqrt(-1.0) != 0.0) return 6;

  double e = pow(2.0, 10.0);
  if (e < 1023.9 || e > 1024.1) return 7;

  double f = pow(2.0, -3.0);
  if (f < 0.124 || f > 0.126) return 8;

  float g = powf(3.0f, 3.0f);
  if (g < 26.9f || g > 27.1f) return 9;

  double h = sin(0.0);
  if (h < -0.01 || h > 0.01) return 10;

  double i = sin(1.57079632679);
  if (i < 0.99 || i > 1.01) return 11;

  double j = cos(0.0);
  if (j < 0.99 || j > 1.01) return 12;

  double k = cos(3.14159265359);
  if (k < -1.01 || k > -0.99) return 13;

  float m = sinf(1.5707963f);
  if (m < 0.99f || m > 1.01f) return 14;

  float n = cosf(0.0f);
  if (n < 0.99f || n > 1.01f) return 15;

  return 0;
}
