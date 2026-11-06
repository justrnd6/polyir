#import <stdint.h>

#import "../Vulkan.h"
#import "shared/pixels.h"

static int64_t main_(void);

static int64_t main_(void) {
  int64_t gpu = vulkanStart();
  if (gpu == 0) {
    return 10;
  }
  printTable(facePicture());
  int64_t code = scanDemo(gpu, "");
  if (code != 0) {
    return code;
  }
  code = channelsDemo(gpu, "");
  if (code != 0) {
    return code;
  }
  code = bitsDemo(gpu, "");
  if (code != 0) {
    return code;
  }
  code = resizeDemo(gpu, "");
  if (code != 0) {
    return code;
  }
  return enlargeDemo(gpu, "");
}

int main(void) {
  return (int) main_();
}
