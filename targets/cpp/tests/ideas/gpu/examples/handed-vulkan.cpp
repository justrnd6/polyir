#include <cstdint>

#include "../Vulkan.hpp"
#include "shared/handed.hpp"

int main() {
  const int64_t gpu = vulkanStart();
  if (gpu == 0) {
    return 10;
  }
  return runAll(gpu);
}
