#pragma once

#include <cstdint>
#include <memory>

#include "shapes.hpp"

int64_t rectArea(int64_t w, int64_t h);

inline int64_t rectArea(int64_t w, int64_t h) {
  return area(std::make_shared<Rect>(w, h));
}
