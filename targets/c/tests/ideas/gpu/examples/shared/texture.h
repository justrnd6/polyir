#pragma once

#include <math.h>
#include <stdint.h>

static int32_t wrap(int32_t n, int32_t size, int32_t mode);
static float texel(float* tex, int32_t offset, int32_t size, int32_t x, int32_t y, int32_t mode);
static float nearest(float* tex, int32_t offset, int32_t size, float u, float v, int32_t mode);
static float lerp(float a, float b, float t);
static float bilinear(float* tex, int32_t offset, int32_t size, float u, float v, int32_t mode);
static int32_t levelOffset(int32_t base, int32_t level);
static int32_t levelSize(int32_t base, int32_t level);
static float mipSample(float* tex, int32_t base, int32_t levels, float u, float v, float level, int32_t mode);
static int64_t pyramidSize(int64_t base);
static int64_t fillBase(float* tex, int64_t base);
static int64_t buildPyramid(float* tex, int64_t base);
static int64_t shrink(float* tex, int64_t from, int64_t size);

static int32_t wrap(int32_t n, int32_t size, int32_t mode) {
  if (mode == 0) {
    if (n < 0) {
      return 0;
    }
    if (n > size - 1) {
      return size - 1;
    }
    return n;
  }
  if (mode == 1) {
    return (n % size + size) % size;
  }
  int32_t period = size * 2;
  int32_t m = (n % period + period) % period;
  if (m >= size) {
    return period - 1 - m;
  }
  return m;
}

static float texel(float* tex, int32_t offset, int32_t size, int32_t x, int32_t y, int32_t mode) {
  return tex[offset + wrap(y, size, mode) * size + wrap(x, size, mode)];
}

static float nearest(float* tex, int32_t offset, int32_t size, float u, float v, int32_t mode) {
  int32_t x = (int32_t) floorf(u * (float) size);
  int32_t y = (int32_t) floorf(v * (float) size);
  return texel(tex, offset, size, x, y, mode);
}

static float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

static float bilinear(float* tex, int32_t offset, int32_t size, float u, float v, int32_t mode) {
  float sx = u * (float) size - 0.5f;
  float sy = v * (float) size - 0.5f;
  int32_t x0 = (int32_t) floorf(sx);
  int32_t y0 = (int32_t) floorf(sy);
  float fx = sx - (float) x0;
  float fy = sy - (float) y0;
  float top = lerp(texel(tex, offset, size, x0, y0, mode), texel(tex, offset, size, x0 + 1, y0, mode), fx);
  float bottom = lerp(texel(tex, offset, size, x0, y0 + 1, mode), texel(tex, offset, size, x0 + 1, y0 + 1, mode), fx);
  return lerp(top, bottom, fy);
}

static int32_t levelOffset(int32_t base, int32_t level) {
  int32_t offset = 0;
  int32_t size = base;
  for (int64_t k = 0; k < (int64_t) level; k = k + 1) {
    offset = offset + size * size;
    size = size / 2;
  }
  return offset;
}

static int32_t levelSize(int32_t base, int32_t level) {
  int32_t size = base;
  for (int64_t k = 0; k < (int64_t) level; k = k + 1) {
    size = size / 2;
  }
  return size;
}

static float mipSample(float* tex, int32_t base, int32_t levels, float u, float v, float level, int32_t mode) {
  int32_t lo = (int32_t) floorf(level);
  if (lo < 0) {
    lo = 0;
  }
  if (lo > levels - 1) {
    lo = levels - 1;
  }
  int32_t hi = lo + 1;
  if (hi > levels - 1) {
    hi = levels - 1;
  }
  float amount = level - (float) lo;
  if (amount < 0.0f) {
    amount = 0.0f;
  }
  if (amount > 1.0f) {
    amount = 1.0f;
  }
  float a = bilinear(tex, levelOffset(base, lo), levelSize(base, lo), u, v, mode);
  float b = bilinear(tex, levelOffset(base, hi), levelSize(base, hi), u, v, mode);
  return lerp(a, b, amount);
}

static int64_t pyramidSize(int64_t base) {
  int64_t total = 0;
  int64_t size = base;
  for (int64_t k = 0; k < 16; k = k + 1) {
    if (size >= 1) {
      total = total + size * size;
      size = size / 2;
    }
  }
  return total;
}

static int64_t fillBase(float* tex, int64_t base) {
  for (int64_t y = 0; y < base; y = y + 1) {
    for (int64_t x = 0; x < base; x = x + 1) {
      float level = (x / 4 + y / 4) % 2 == 1 ? 0.45f : 0.85f;
      if (x % 4 == 0 || y % 4 == 0) {
        level = level * 0.6f;
      }
      tex[y * base + x] = level;
    }
  }
  return 0;
}

static int64_t buildPyramid(float* tex, int64_t base) {
  fillBase(tex, base);
  int64_t from = 0;
  int64_t size = base;
  for (int64_t level = 0; level < 16; level = level + 1) {
    if (size > 1) {
      shrink(tex, from, size);
      from = from + size * size;
      size = size / 2;
    }
  }
  return 0;
}

static int64_t shrink(float* tex, int64_t from, int64_t size) {
  int64_t next = from + size * size;
  int64_t half = size / 2;
  for (int64_t y = 0; y < half; y = y + 1) {
    for (int64_t x = 0; x < half; x = x + 1) {
      float a = tex[from + 2 * y * size + 2 * x];
      float b = tex[from + 2 * y * size + 2 * x + 1];
      float c = tex[from + (2 * y + 1) * size + 2 * x];
      float d = tex[from + (2 * y + 1) * size + 2 * x + 1];
      tex[next + y * half + x] = (a + b + c + d) * 0.25f;
    }
  }
  return 0;
}
