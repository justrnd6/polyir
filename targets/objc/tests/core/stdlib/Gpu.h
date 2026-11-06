#pragma once

#import <stdint.h>

static int32_t atomicAdd(int32_t* counts, int32_t slot, int32_t amount);
static float atomicAddFloat(float* sums, int32_t slot, float amount);
static int32_t atomicMin(int32_t* values, int32_t slot, int32_t candidate);
static int32_t atomicMax(int32_t* values, int32_t slot, int32_t candidate);
static int32_t groupSize(void);
static int32_t localIndex(int32_t i);
static int32_t groupIndex(int32_t i);
static int32_t barrier(void);

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
