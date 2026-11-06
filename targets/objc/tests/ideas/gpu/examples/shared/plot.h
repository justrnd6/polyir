#pragma once

#import <stdint.h>

static int64_t drawStems(float* pic, int64_t w, int64_t h, float* values, int64_t n, float low, float high);
static int64_t minOf(int64_t a, int64_t b);
static int64_t maxOf(int64_t a, int64_t b);
static int64_t drawWave(float* pic, int64_t w, int64_t h, float* values, int64_t n, float low, float high);
static int64_t drawCurve(float* pic, int64_t w, int64_t h, float* values, int64_t n, float low, float high);
static int64_t curveRow(float value, int64_t h, float low, float high);

static int64_t drawStems(float* pic, int64_t w, int64_t h, float* values, int64_t n, float low, float high) {
  for (int64_t k = 0; k < w * h; k = k + 1) {
    pic[k] = 0.96f;
  }
  int64_t zero = (int64_t) (high / (high - low) * (float) h);
  for (int64_t x = 0; x < w; x = x + 1) {
    pic[zero * w + x] = 0.55f;
  }
  for (int64_t j = 0; j < n; j = j + 1) {
    int64_t column = (int64_t) (((float) j + 0.5f) * (float) w / (float) n);
    int64_t top = (int64_t) ((high - values[j]) / (high - low) * (float) (h - 1));
    if (top < 0) {
      top = 0;
    }
    if (top > h - 1) {
      top = h - 1;
    }
    int64_t from = minOf(top, zero);
    int64_t to = maxOf(top, zero);
    for (int64_t y = from; y <= to; y = y + 1) {
      pic[y * w + column] = 0.1f;
    }
    for (int64_t dy = 0 - 1; dy <= 1; dy = dy + 1) {
      for (int64_t dx = 0 - 1; dx <= 1; dx = dx + 1) {
        int64_t yy = top + dy;
        int64_t xx = column + dx;
        if (yy >= 0 && yy < h && xx >= 0 && xx < w) {
          pic[yy * w + xx] = 0.1f;
        }
      }
    }
  }
  return 0;
}

static int64_t minOf(int64_t a, int64_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

static int64_t maxOf(int64_t a, int64_t b) {
  if (a > b) {
    return a;
  }
  return b;
}

static int64_t drawWave(float* pic, int64_t w, int64_t h, float* values, int64_t n, float low, float high) {
  for (int64_t k = 0; k < w * h; k = k + 1) {
    pic[k] = 0.96f;
  }
  int64_t zero = (int64_t) (high / (high - low) * (float) h);
  for (int64_t x = 0; x < w; x = x + 1) {
    pic[zero * w + x] = 0.55f;
  }
  for (int64_t x = 0; x < w; x = x + 1) {
    int64_t from = x * n / w;
    int64_t upto = (x + 1) * n / w;
    if (upto <= from) {
      upto = from + 1;
    }
    float lowest = values[from];
    float highest = values[from];
    for (int64_t j = from; j < upto; j = j + 1) {
      if (values[j] < lowest) {
        lowest = values[j];
      }
      if (values[j] > highest) {
        highest = values[j];
      }
    }
    int64_t top = (int64_t) ((high - highest) / (high - low) * (float) (h - 1));
    int64_t bottom = (int64_t) ((high - lowest) / (high - low) * (float) (h - 1));
    if (top < 0) {
      top = 0;
    }
    if (bottom > h - 1) {
      bottom = h - 1;
    }
    for (int64_t y = top; y <= bottom; y = y + 1) {
      pic[y * w + x] = 0.1f;
    }
  }
  return 0;
}

static int64_t drawCurve(float* pic, int64_t w, int64_t h, float* values, int64_t n, float low, float high) {
  for (int64_t k = 0; k < w * h; k = k + 1) {
    pic[k] = 0.96f;
  }
  int64_t zero = (int64_t) (high / (high - low) * (float) h);
  for (int64_t x = 0; x < w; x = x + 1) {
    pic[zero * w + x] = 0.55f;
  }
  int64_t previous = curveRow(values[0], h, low, high);
  for (int64_t x = 0; x < w; x = x + 1) {
    int64_t row = curveRow(values[x * n / w], h, low, high);
    for (int64_t y = minOf(row, previous); y <= maxOf(row, previous); y = y + 1) {
      pic[y * w + x] = 0.1f;
    }
    previous = row;
  }
  return 0;
}

static int64_t curveRow(float value, int64_t h, float low, float high) {
  int64_t row = (int64_t) ((high - value) / (high - low) * (float) (h - 1));
  if (row < 0) {
    row = 0;
  }
  if (row > h - 1) {
    row = h - 1;
  }
  return row;
}
