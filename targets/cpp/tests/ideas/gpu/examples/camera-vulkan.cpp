#include <cstdint>
#include <string>
using namespace std::string_literals;

#include "../Vulkan.hpp"
#include "shared/camera.hpp"

int main() {
  const int64_t gpu = vulkanStart();
  if (gpu == 0) {
    return 10;
  }
  return runAll(gpu, ""s);
}
