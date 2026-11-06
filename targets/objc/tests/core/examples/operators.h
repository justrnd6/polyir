#pragma once

#import <stdint.h>

static int64_t bitwisePrecedenceCheck(void);
static int64_t shiftPrecedenceCheck(void);

static int64_t bitwisePrecedenceCheck(void) {
  int64_t a = 6;
  int64_t b = 3;
  int64_t c = 1;
  int64_t d = 2;
  return a | b & c ^ d;
}

static int64_t shiftPrecedenceCheck(void) {
  int64_t x = 1;
  return x << 2 & 3;
}
