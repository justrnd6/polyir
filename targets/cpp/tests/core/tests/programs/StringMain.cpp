#include <cstdint>

#include "StringTests.hpp"

int64_t failedIf(double off);

inline int64_t failedIf(double off) {
  if (off > 0.001 || off < 0.0 - 0.001) {
    return 1;
  }
  return 0;
}

int main() {
  int64_t failed = 0;
  failed = failed + failedIf(checkEscapes());
  failed = failed + failedIf(checkCharEscapes());
  failed = failed + failedIf(checkInterpolation());
  failed = failed + failedIf(checkInterpolatedMethodCalls());
  failed = failed + failedIf(checkTextBlock());
  failed = failed + failedIf(checkTextBlockWithoutFinalNewline());
  failed = failed + failedIf(checkTextBlockKeepsIndentOfMultiLineValue());
  failed = failed + failedIf(checkStrOfValues());
  failed = failed + failedIf(checkCharAt());
  failed = failed + failedIf(checkSliceAndFind());
  failed = failed + failedIf(checkSliceClamps());
  failed = failed + failedIf(checkFindFrom());
  failed = failed + failedIf(checkListLiteralAndForIn());
  failed = failed + failedIf(checkSplitAndJoin());
  failed = failed + failedIf(checkTrimAndContains());
  failed = failed + failedIf(checkStartsAndEnds());
  failed = failed + failedIf(checkReplaceRepeatIndent());
  failed = failed + failedIf(checkLines());
  failed = failed + failedIf(checkFill());
  failed = failed + failedIf(checkStringBuilder());
  return failed;
}
