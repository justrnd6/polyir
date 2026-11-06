#pragma once

#include <stdint.h>

static int32_t queens(int32_t t, int32_t* prefixes, int32_t* p, int32_t* stack);
static int64_t listPrefixes(int64_t n, int64_t depth, int32_t* out);

static int32_t queens(int32_t t, int32_t* prefixes, int32_t* p, int32_t* stack) {
  int32_t n = p[0];
  int32_t depth = p[1];
  int32_t full = (int32_t) ((1 << (int64_t) n) - 1);
  int32_t base = t * (n + 1) * 4;
  stack[base] = prefixes[t * 3];
  stack[base + 1] = prefixes[t * 3 + 1];
  stack[base + 2] = prefixes[t * 3 + 2];
  stack[base + 3] = full & ~(prefixes[t * 3] | prefixes[t * 3 + 1] | prefixes[t * 3 + 2]);
  int32_t level = 0;
  int32_t count = 0;
  for (int64_t step = 0; level >= 0; step = step + 1) {
    int32_t at = base + level * 4;
    int32_t free = stack[at + 3];
    if (free == 0) {
      level = level - 1;
    } else {
      int32_t bit = free & (int32_t) (0 - (int64_t) free);
      stack[at + 3] = free ^ bit;
      if (depth + level + 1 == n) {
        count = count + 1;
      } else {
        int32_t nextCols = stack[at] | bit;
        int32_t nextLeft = (stack[at + 1] | bit) << 1 & full;
        int32_t nextRight = (stack[at + 2] | bit) >> 1;
        level = level + 1;
        int32_t below = base + level * 4;
        stack[below] = nextCols;
        stack[below + 1] = nextLeft;
        stack[below + 2] = nextRight;
        stack[below + 3] = full & ~(nextCols | nextLeft | nextRight);
      }
    }
  }
  return count;
}

static int64_t listPrefixes(int64_t n, int64_t depth, int32_t* out) {
  int64_t total = 1;
  int64_t r = 0;
  while (r < depth) {
    total = total * n;
    r = r + 1;
  }
  int32_t full = (int32_t) ((1 << n) - 1);
  int64_t found = 0;
  for (int64_t code = 0; code < total; code = code + 1) {
    int32_t left = 0;
    int32_t right = 0;
    int32_t cols = 0;
    int64_t ok = 1;
    int64_t rest = code;
    for (int64_t r2 = 0; r2 < depth; r2 = r2 + 1) {
      int64_t column = rest % n;
      rest = rest / n;
      int32_t bit = (int32_t) (1 << column);
      if ((bit & (cols | left | right)) != 0) {
        ok = 0;
      }
      cols = cols | bit;
      left = (left | bit) << 1 & full;
      right = (right | bit) >> 1;
    }
    if (ok == 1) {
      out[found * 3] = cols;
      out[found * 3 + 1] = left;
      out[found * 3 + 2] = right;
      found = found + 1;
    }
  }
  return found;
}
