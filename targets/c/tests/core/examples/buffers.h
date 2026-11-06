#pragma once

#include <stdint.h>

static double sumBuf(double* buf, int64_t len);

static double sumBuf(double* buf, int64_t len) {
  int64_t i = 0;
  double total = 0.0;
  while (i < len) {
    i = i + 1;
    total = total + buf[i];
  }
  return total;
}
