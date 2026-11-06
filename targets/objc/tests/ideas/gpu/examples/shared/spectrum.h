#pragma once

#import <math.h>
#import <stdint.h>

static float dft(int32_t k, float* x, float* p);

static float dft(int32_t k, float* x, float* p) {
  int32_t n = (int32_t) p[0];
  float re = 0.0f;
  float im = 0.0f;
  for (int64_t j = 0; j < (int64_t) n; j = j + 1) {
    float turns = (float) (k * (int32_t) j % n) / (float) n;
    re = re + x[j] * cosf((float) (6.2831853 * (double) turns));
    im = im - x[j] * sinf((float) (6.2831853 * (double) turns));
  }
  return (float) (2.0 * (double) sqrtf(re * re + im * im) / (double) (float) n);
}
