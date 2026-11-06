#include <cstdint>
#include <string>
using namespace std::string_literals;

#include "../Vulkan.hpp"
#include "shared/pixels.hpp"

int main() {
  const int64_t gpu = vulkanStart();
  if (gpu == 0) {
    return 10;
  }
  printTable(facePicture());
  int64_t code = scanDemo(gpu, ""s);
  if (code != 0) {
    return code;
  }
  code = channelsDemo(gpu, ""s);
  if (code != 0) {
    return code;
  }
  code = bitsDemo(gpu, ""s);
  if (code != 0) {
    return code;
  }
  code = resizeDemo(gpu, ""s);
  if (code != 0) {
    return code;
  }
  return enlargeDemo(gpu, ""s);
}
