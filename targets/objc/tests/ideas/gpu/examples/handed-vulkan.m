#import <stdint.h>

#import "../Vulkan.h"
#import "shared/handed.h"

static int64_t main_(void);

static int64_t main_(void) {
  int64_t gpu = vulkanStart();
  if (gpu == 0) {
    return 10;
  }
  return runAll(gpu);
}

int main(void) {
  return (int) main_();
}
