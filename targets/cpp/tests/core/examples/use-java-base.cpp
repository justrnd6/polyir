#include <cstdint>

struct Runnable;

struct Runnable {
  virtual ~Runnable() = default;

  virtual int64_t run() = 0;
};

int main() {
  return 0;
}
