#include <cstdint>

#include "MatchTests.hpp"

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
  failed = failed + failedIf(checkComparisons());
  failed = failed + failedIf(checkGuardsOnTheSubject());
  failed = failed + failedIf(checkStrings());
  failed = failed + failedIf(checkConditions());
  failed = failed + failedIf(checkBooleans());
  failed = failed + failedIf(checkOptionals());
  failed = failed + failedIf(checkEithers());
  failed = failed + failedIf(checkVariants());
  failed = failed + failedIf(checkPayloadPatterns());
  failed = failed + failedIf(checkLists());
  failed = failed + failedIf(checkListGuards());
  failed = failed + failedIf(checkStatements());
  failed = failed + failedIf(checkNested());
  return failed;
}
