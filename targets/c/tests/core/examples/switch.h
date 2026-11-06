#pragma once

#include <stdint.h>

static int64_t dayKind(int64_t day);

static int64_t dayKind(int64_t day) {
  int64_t label = 0;
  for (;;) {
    switch (label) {
      case 0: {
        if (day == 0) {
          label = 1;
          continue;
        } else if (day == 6) {
          label = 1;
          continue;
        } else if (day == 1) {
          label = 2;
          continue;
        } else if (day == 2) {
          label = 2;
          continue;
        } else if (day == 3) {
          label = 2;
          continue;
        } else if (day == 4) {
          label = 2;
          continue;
        } else if (day == 5) {
          label = 2;
          continue;
        } else {
          label = 3;
          continue;
        }
      }
      case 1: {
        return 0;
      }
      case 2: {
        return 1;
      }
      case 3: {
        return 0 - 1;
      }
    }
  }
}
