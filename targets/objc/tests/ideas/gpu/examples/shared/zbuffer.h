#pragma once

#import <math.h>
#import <stdint.h>

static int32_t atomicMin(int32_t* values, int32_t slot, int32_t candidate);
static int32_t emptyKey(void);
static int32_t rasterize(int32_t t, float* tris, int32_t width, int32_t height, int32_t* depth);
static float shade(int32_t p, int32_t* depth, float* tris);
static int64_t makeScene(float* tris, int64_t count, int64_t width, int64_t height);
static int64_t nextSeed(int64_t seed);
static float nextRandom(int64_t seed);

static int32_t atomicMin(int32_t* values, int32_t slot, int32_t candidate) {
  int32_t old = values[slot];
  if (candidate < old) {
    values[slot] = candidate;
  }
  return old;
}

static int32_t emptyKey(void) {
  return 2147483647;
}

static int32_t rasterize(int32_t t, float* tris, int32_t width, int32_t height, int32_t* depth) {
  int32_t base = t * 10;
  float x0 = tris[base];
  float y0 = tris[base + 1];
  float z0 = tris[base + 2];
  float x1 = tris[base + 3];
  float y1 = tris[base + 4];
  float z1 = tris[base + 5];
  float x2 = tris[base + 6];
  float y2 = tris[base + 7];
  float z2 = tris[base + 8];
  float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  int32_t left = (int32_t) fmaxf(0.0f, floorf(fminf(x0, fminf(x1, x2))));
  int32_t right = (int32_t) fminf((float) width - 1.0f, ceilf(fmaxf(x0, fmaxf(x1, x2))));
  int32_t top = (int32_t) fmaxf(0.0f, floorf(fminf(y0, fminf(y1, y2))));
  int32_t bottom = (int32_t) fminf((float) height - 1.0f, ceilf(fmaxf(y0, fmaxf(y1, y2))));
  for (int32_t y = top; y <= bottom; y = y + 1) {
    for (int32_t x = left; x <= right; x = x + 1) {
      float px = (float) x + 0.5f;
      float py = (float) y + 0.5f;
      float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area;
      float w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area;
      float w2 = (float) (1.0 - (double) w0 - (double) w1);
      if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
        float z = w0 * z0 + w1 * z1 + w2 * z2;
        atomicMin(depth, y * width + x, (int32_t) (z * 1048575.0f) * 512 + t);
      }
    }
  }
  return 0;
}

static float shade(int32_t p, int32_t* depth, float* tris) {
  int32_t key = depth[p];
  if (key == 2147483647) {
    return 0.06f;
  }
  int32_t id_ = key % 512;
  float z = (float) (key / 512) / 1048575.0f;
  return tris[id_ * 10 + 9] * (float) (1.0 - 0.45 * (double) z);
}

static int64_t makeScene(float* tris, int64_t count, int64_t width, int64_t height) {
  int64_t seed = 12345;
  for (int64_t t = 0; t < count; t = t + 1) {
    float cx = nextRandom(seed) * (float) width;
    seed = nextSeed(seed);
    float cy = nextRandom(seed) * (float) height;
    seed = nextSeed(seed);
    float size = (float) (30.0 + (double) (nextRandom(seed) * 110.0f));
    seed = nextSeed(seed);
    for (int64_t corner = 0; corner < 3; corner = corner + 1) {
      tris[t * 10 + corner * 3] = cx + (nextRandom(seed) - 0.5f) * 2.0f * size;
      seed = nextSeed(seed);
      tris[t * 10 + corner * 3 + 1] = cy + (nextRandom(seed) - 0.5f) * 2.0f * size;
      seed = nextSeed(seed);
      tris[t * 10 + corner * 3 + 2] = nextRandom(seed);
      seed = nextSeed(seed);
    }
    tris[t * 10 + 9] = (float) (0.25 + 0.75 * (double) nextRandom(seed));
    seed = nextSeed(seed);
  }
  return 0;
}

static int64_t nextSeed(int64_t seed) {
  return (seed * 1103515245 + 12345) % 2147483648;
}

static float nextRandom(int64_t seed) {
  return (float) (seed / 65536 % 32768) / 32768.0f;
}
