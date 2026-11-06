#pragma once

#include <stdint.h>

static int64_t counter = 0;

static int64_t bump(void);

static int64_t bump(void) {
  counter = counter + 1;
  return counter;
}
