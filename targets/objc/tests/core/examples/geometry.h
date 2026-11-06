#pragma once

#import <stdint.h>

#import "shapes.h"

static int64_t rectArea(int64_t w, int64_t h);

static int64_t rectArea(int64_t w, int64_t h) {
  return area(Rect_new(w, h));
}
