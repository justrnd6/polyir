#pragma once

#include <stdint.h>

static int64_t gcd(int64_t a0, int64_t b0);

static int64_t gcd(int64_t a0, int64_t b0) {
  int64_t label = 0;
  int64_t a = 0;
  int64_t b = 0;
  int64_t result = 0;
  for (;;) {
    switch (label) {
      case 0: {
        a = a0;
        b = b0;
        label = 1;
        continue;
      }
      case 1: {
        if (b == 0) {
          result = a;
          label = 3;
          continue;
        } else {
          label = 2;
          continue;
        }
      }
      case 2: {
        int64_t r = a % b;
        a = b;
        b = r;
        label = 1;
        continue;
      }
      case 3: {
        return result;
      }
    }
  }
}
