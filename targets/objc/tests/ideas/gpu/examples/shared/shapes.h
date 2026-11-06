#pragma once

#import <math.h>
#import <stdint.h>

static int32_t atomicAdd(int32_t* counts, int32_t slot, int32_t amount);
static float atomicAddFloat(float* sums, int32_t slot, float amount);
static int32_t atomicMin(int32_t* values, int32_t slot, int32_t candidate);
static int32_t atomicMax(int32_t* values, int32_t slot, int32_t candidate);
static float blend7(float a, float b, float c, float d, float e, float f, float g, float k);
static float plasma(int32_t x, int32_t y, int32_t width, int32_t height, float phase);
static float vignette(float pixel, int32_t x, int32_t y, int32_t width, int32_t height);
static int32_t tally(int32_t i, int32_t* counts, float* sums, int32_t* lows, int32_t* highs);
static int32_t reported(int32_t i);
static int64_t nearly(float a, float b, float tolerance);

static int32_t atomicAdd(int32_t* counts, int32_t slot, int32_t amount) {
  int32_t old = counts[slot];
  counts[slot] = old + amount;
  return old;
}

static float atomicAddFloat(float* sums, int32_t slot, float amount) {
  float old = sums[slot];
  sums[slot] = old + amount;
  return old;
}

static int32_t atomicMin(int32_t* values, int32_t slot, int32_t candidate) {
  int32_t old = values[slot];
  if (candidate < old) {
    values[slot] = candidate;
  }
  return old;
}

static int32_t atomicMax(int32_t* values, int32_t slot, int32_t candidate) {
  int32_t old = values[slot];
  if (candidate > old) {
    values[slot] = candidate;
  }
  return old;
}

static float blend7(float a, float b, float c, float d, float e, float f, float g, float k) {
  return k * (a + (float) (2.0 * (double) b) + (float) (3.0 * (double) c)) + (d - e) * (f + g);
}

static float plasma(int32_t x, int32_t y, int32_t width, int32_t height, float phase) {
  float u = (float) x / (float) width;
  float v = (float) y / (float) height;
  float a = sinf((float) (10.0 * (double) u + (double) phase));
  float b = sinf((float) (8.0 * (double) v - (double) phase));
  float c = sinf((float) (6.0 * (double) (u + v) + 2.0 * (double) phase));
  return (float) (0.5 + 0.1667 * (double) (a + b + c));
}

static float vignette(float pixel, int32_t x, int32_t y, int32_t width, int32_t height) {
  float dx = (float) x / (float) width - 0.5f;
  float dy = (float) y / (float) height - 0.5f;
  float edge = (float) (1.0 - 1.6 * (double) (dx * dx + dy * dy));
  return pixel * edge;
}

static int32_t tally(int32_t i, int32_t* counts, float* sums, int32_t* lows, int32_t* highs) {
  int32_t value = i * 7919 % 10007;
  atomicAdd(counts, i % 4, 1);
  atomicAddFloat(sums, i % 4, 0.5f);
  atomicMin(lows, 0, value);
  atomicMax(highs, 0, value);
  return 0;
}

static int32_t reported(int32_t i) {
  return i * 7919 % 10007;
}

static int64_t nearly(float a, float b, float tolerance) {
  float gap = a - b;
  if (gap < 0.0f) {
    gap = (float) (0.0 - (double) gap);
  }
  if (gap <= tolerance) {
    return 1;
  }
  return 0;
}
