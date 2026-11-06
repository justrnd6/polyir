#pragma once

#import <stdint.h>

static double vecMulSum(double a, double b);
static int64_t sumI64Vec(int64_t a, int64_t b, int64_t c);
static double vecMulSumViaVar(double a, double b);

static double vecMulSum(double a, double b) {
  double __attribute__((vector_size(4 * sizeof(double)))) v1 = (double __attribute__((vector_size(4 * sizeof(double))))){a, a + 1.0, a + 2.0, a + 3.0};
  double __attribute__((vector_size(4 * sizeof(double)))) v2 = (double __attribute__((vector_size(4 * sizeof(double))))){b, b, b, b};
  double __attribute__((vector_size(4 * sizeof(double)))) p = v1 * v2;
  return p[0] + p[1] + p[2] + p[3];
}

static int64_t sumI64Vec(int64_t a, int64_t b, int64_t c) {
  int64_t __attribute__((vector_size(3 * sizeof(int64_t)))) v = (int64_t __attribute__((vector_size(3 * sizeof(int64_t))))){a, b, c};
  return v[0] + v[1] + v[2];
}

static double vecMulSumViaVar(double a, double b) {
  double __attribute__((vector_size(4 * sizeof(double)))) v1 = (double __attribute__((vector_size(4 * sizeof(double))))){a, a + 1.0, a + 2.0, a + 3.0};
  double __attribute__((vector_size(4 * sizeof(double)))) v2 = (double __attribute__((vector_size(4 * sizeof(double))))){b, b, b, b};
  double __attribute__((vector_size(4 * sizeof(double)))) p = v1 * v2;
  double total = 0.0;
  for (int64_t i = 0; i < 4; i = i + 1) {
    total = total + p[i];
  }
  return total;
}
