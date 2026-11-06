#pragma once

#include <math.h>
#include <stdint.h>

static int32_t fftStage(int32_t t, float* src, int32_t* p, float* dst);
static float fftMagnitude(int32_t k, float* data, int32_t* p);
static int32_t fftFrames(int32_t t, float* signal_, int32_t* p, float* dst);
static float spectrogramPixel(int32_t i, float* data, int32_t* p);
static int32_t complexMultiply(int32_t k, float* a, float* b, float* out);
static float realScaled(int32_t i, float* data, float* p);

static int32_t fftStage(int32_t t, float* src, int32_t* p, float* dst) {
  int32_t total = p[0];
  int32_t n = p[1];
  int32_t s = p[2];
  int32_t half = total / 2;
  int32_t batch = t / half;
  int32_t local = t % half;
  int32_t position = local / s;
  int32_t column = local % s;
  int32_t base = batch * total * 2;
  int32_t a = base + (int32_t) (2 * (int64_t) (column + s * position));
  int32_t b = base + (int32_t) (2 * (int64_t) (column + s * (position + n / 2)));
  float sumRe = src[a] + src[b];
  float sumIm = src[a + 1] + src[b + 1];
  float diffRe = src[a] - src[b];
  float diffIm = src[a + 1] - src[b + 1];
  float angle = (float) ((0.0 - 6.2831853) * (double) (float) p[3] * (double) (float) position / (double) (float) n);
  float twiddleRe = cosf(angle);
  float twiddleIm = sinf(angle);
  int32_t first = base + (int32_t) (2 * (int64_t) (column + s * (int32_t) (2 * (int64_t) position)));
  int32_t second = base + (int32_t) (2 * (int64_t) (column + s * (int32_t) (2 * (int64_t) position + 1)));
  dst[first] = sumRe;
  dst[first + 1] = sumIm;
  dst[second] = diffRe * twiddleRe - diffIm * twiddleIm;
  dst[second + 1] = diffRe * twiddleIm + diffIm * twiddleRe;
  return 0;
}

static float fftMagnitude(int32_t k, float* data, int32_t* p) {
  int32_t total = p[0];
  float re = data[2 * (int64_t) k];
  float im = data[2 * (int64_t) k + 1];
  return (float) (2.0 * (double) sqrtf(re * re + im * im) / (double) (float) total);
}

static int32_t fftFrames(int32_t t, float* signal_, int32_t* p, float* dst) {
  int32_t total = p[0];
  int32_t frame = t / total;
  int32_t k = t % total;
  float window = (float) (0.5 - 0.5 * (double) cosf((float) (6.2831853 * (double) (float) k / (double) (float) (total - 1))));
  dst[2 * (int64_t) t] = signal_[frame * p[1] + k] * window;
  dst[2 * (int64_t) t + 1] = 0.0f;
  return 0;
}

static float spectrogramPixel(int32_t i, float* data, int32_t* p) {
  int32_t total = p[0];
  int32_t frames = p[1];
  int32_t rows = p[2];
  int32_t x = i % frames;
  int32_t bin = rows - 1 - i / frames;
  int32_t at = x * total * 2 + (int32_t) (2 * (int64_t) bin);
  float re = data[at];
  float im = data[at + 1];
  float magnitude = (float) (4.0 * (double) sqrtf(re * re + im * im) / (double) (float) total);
  float level = (float) ((20.0 * (double) logf(magnitude + 0.0000001f) / 2.3025851 + 70.0) / 70.0);
  if (level < 0.0f) {
    level = 0.0f;
  }
  if (level > 1.0f) {
    level = 1.0f;
  }
  return level;
}

static int32_t complexMultiply(int32_t k, float* a, float* b, float* out) {
  out[2 * (int64_t) k] = a[2 * (int64_t) k] * b[2 * (int64_t) k] - a[2 * (int64_t) k + 1] * b[2 * (int64_t) k + 1];
  out[2 * (int64_t) k + 1] = a[2 * (int64_t) k] * b[2 * (int64_t) k + 1] + a[2 * (int64_t) k + 1] * b[2 * (int64_t) k];
  return 0;
}

static float realScaled(int32_t i, float* data, float* p) {
  return data[2 * (int64_t) i] / p[0];
}
