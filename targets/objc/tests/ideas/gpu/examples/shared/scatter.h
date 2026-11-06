#pragma once

#import <math.h>
#import <stdint.h>

static int32_t atomicAdd(int32_t* counts, int32_t slot, int32_t amount);
static int32_t reverseCopy(int32_t i, float* src, int32_t n, float* dst);
static int32_t histogram(int32_t i, int32_t* data, int32_t bins, int32_t* counts);
static int32_t histogramRacy(int32_t i, int32_t* data, int32_t bins, int32_t* counts);
static int32_t splat(int32_t i, float* p, int32_t* cells);

static int32_t atomicAdd(int32_t* counts, int32_t slot, int32_t amount) {
  int32_t old = counts[slot];
  counts[slot] = old + amount;
  return old;
}

static int32_t reverseCopy(int32_t i, float* src, int32_t n, float* dst) {
  dst[n - 1 - i] = src[i];
  return 0;
}

static int32_t histogram(int32_t i, int32_t* data, int32_t bins, int32_t* counts) {
  int32_t bin = data[i] % bins;
  atomicAdd(counts, bin, 1);
  return 0;
}

static int32_t histogramRacy(int32_t i, int32_t* data, int32_t bins, int32_t* counts) {
  int32_t bin = data[i] % bins;
  counts[bin] = counts[bin] + 1;
  return 0;
}

static int32_t splat(int32_t i, float* p, int32_t* cells) {
  int32_t w = (int32_t) p[0];
  int32_t arm = i % 2;
  float t = (float) (i / 2 % 20000) / 20000.0f;
  float angle = t * 9.0f + (float) arm * 3.14159265f;
  float radius = (float) (0.08 + 0.85 * (double) t);
  int32_t h1 = (i * 75 + 74) % 65537;
  int32_t h2 = (h1 * 75 + 74) % 65537;
  float spread = (float) (0.015 + 0.09 * (double) t);
  float x = radius * cosf(angle) + ((float) h1 / 65537.0f - 0.5f) * spread * 2.0f;
  float y = radius * sinf(angle) + ((float) h2 / 65537.0f - 0.5f) * spread * 2.0f;
  int32_t px = (int32_t) floorf((x * 0.5f + 0.5f) * (float) w);
  int32_t py = (int32_t) floorf((y * 0.5f + 0.5f) * (float) w);
  if (px >= 0 && px < w && py >= 0 && py < w) {
    atomicAdd(cells, py * w + px, 1);
  }
  return 0;
}
