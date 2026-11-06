#include <cstdint>
#include <string>
using namespace std::string_literals;

#include "../Metal.hpp"
#include "shared/pixels.hpp"

int main() {
  const int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const std::string source = metalReadFile("pixels.metal"s);
  if (source == ""s) {
    return 11;
  }
  printTable(facePicture());
  int64_t code = scanDemo(device, source);
  if (code != 0) {
    return code;
  }
  code = channelsDemo(device, source);
  if (code != 0) {
    return code;
  }
  code = bitsDemo(device, source);
  if (code != 0) {
    return code;
  }
  code = resizeDemo(device, source);
  if (code != 0) {
    return code;
  }
  return enlargeDemo(device, source);
}
