#include <cstdint>

#include "SwitchTests.hpp"

int64_t failedIf(double off);

inline int64_t failedIf(double off) {
  if (off > 0.001 || off < 0.0 - 0.001) {
    return 1;
  }
  return 0;
}

int main() {
  int64_t failed = 0;
  failed = failed + failedIf(checkNumbers());
  failed = failed + failedIf(checkChars());
  failed = failed + failedIf(checkStrings());
  failed = failed + failedIf(checkBooleans());
  failed = failed + failedIf(checkSums());
  failed = failed + failedIf(checkConditionsAmongKeys());
  failed = failed + failedIf(checkCallSubject());
  failed = failed + failedIf(checkConditions());
  return failed;
}
