#pragma once

#import <math.h>
#import <stdint.h>

static float atomicAddFloat(float* sums, int32_t slot, float amount);
static int32_t groupSize(void);
static int32_t localIndex(int32_t i);
static int32_t groupIndex(int32_t i);
static int32_t barrier(void);
static int32_t blockSum(int32_t i, float* src, int32_t n, float* tile, float* partial);
static int32_t blockMax(int32_t i, float* src, int32_t n, float* tile, float* partial);
static int32_t total(int32_t i, float* src, int32_t n, float* tile, float* sum);
static int64_t groupsFor(int64_t n);
static int64_t launchCount(int64_t n);

static float atomicAddFloat(float* sums, int32_t slot, float amount) {
  float old = sums[slot];
  sums[slot] = old + amount;
  return old;
}

static int32_t groupSize(void) {
  return 1;
}

static int32_t localIndex(int32_t i) {
  return 0;
}

static int32_t groupIndex(int32_t i) {
  return i;
}

static int32_t barrier(void) {
  return 0;
}

static int32_t blockSum(int32_t i, float* src, int32_t n, float* tile, float* partial) {
  int32_t lane = localIndex(i);
  if (i < n) {
    tile[lane] = src[i];
  } else {
    tile[lane] = 0.0f;
  }
  barrier();
  for (int32_t s = groupSize() / 2; s > 0; s = s / 2) {
    if (lane < s) {
      tile[lane] = tile[lane] + tile[lane + s];
    }
    barrier();
  }
  if (lane == 0) {
    partial[groupIndex(i)] = tile[0];
  }
  return 0;
}

static int32_t blockMax(int32_t i, float* src, int32_t n, float* tile, float* partial) {
  int32_t lane = localIndex(i);
  if (i < n) {
    tile[lane] = src[i];
  } else {
    tile[lane] = src[0];
  }
  barrier();
  for (int32_t s = groupSize() / 2; s > 0; s = s / 2) {
    if (lane < s) {
      tile[lane] = fmaxf(tile[lane], tile[lane + s]);
    }
    barrier();
  }
  if (lane == 0) {
    partial[groupIndex(i)] = tile[0];
  }
  return 0;
}

static int32_t total(int32_t i, float* src, int32_t n, float* tile, float* sum) {
  int32_t lane = localIndex(i);
  if (i < n) {
    tile[lane] = src[i];
  } else {
    tile[lane] = 0.0f;
  }
  barrier();
  for (int32_t s = groupSize() / 2; s > 0; s = s / 2) {
    if (lane < s) {
      tile[lane] = tile[lane] + tile[lane + s];
    }
    barrier();
  }
  if (lane == 0) {
    atomicAddFloat(sum, 0, tile[0]);
  }
  return 0;
}

static int64_t groupsFor(int64_t n) {
  return (n + 255) / 256;
}

static int64_t launchCount(int64_t n) {
  int64_t launches = 0;
  int64_t left = n;
  for (int64_t k = 0; k < 10; k = k + 1) {
    if (left > 1) {
      left = groupsFor(left);
      launches = launches + 1;
    }
  }
  return launches;
}
