#include <cstdint>
#include <string>
using namespace std::string_literals;

#include "../Metal.hpp"
#include "shared/camera.hpp"

int main() {
  const int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const std::string source = metalReadFile("camera.metal"s);
  if (source == ""s) {
    return 11;
  }
  return runAll(device, source);
}
