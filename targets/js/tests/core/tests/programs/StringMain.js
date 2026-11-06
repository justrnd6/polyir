"use strict";

import { StringBuilder, StringBuilder_line, checkCharAt, checkCharEscapes, checkEscapes, checkFill, checkFindFrom, checkInterpolatedMethodCalls, checkInterpolation, checkLines, checkListLiteralAndForIn, checkReplaceRepeatIndent, checkSliceAndFind, checkSliceClamps, checkSplitAndJoin, checkStartsAndEnds, checkStrOfValues, checkStringBuilder, checkTextBlock, checkTextBlockKeepsIndentOfMultiLineValue, checkTextBlockWithoutFinalNewline, checkTrimAndContains } from "./StringTests.js";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function failedIf(off) {
  if (off > 0.001 || off < 0.0 - 0.001) {
    return 1n;
  }
  return 0n;
}

function main() {
  let failed = 0n;
  failed = i64(failed + failedIf(checkEscapes()));
  failed = i64(failed + failedIf(checkCharEscapes()));
  failed = i64(failed + failedIf(checkInterpolation()));
  failed = i64(failed + failedIf(checkInterpolatedMethodCalls()));
  failed = i64(failed + failedIf(checkTextBlock()));
  failed = i64(failed + failedIf(checkTextBlockWithoutFinalNewline()));
  failed = i64(failed + failedIf(checkTextBlockKeepsIndentOfMultiLineValue()));
  failed = i64(failed + failedIf(checkStrOfValues()));
  failed = i64(failed + failedIf(checkCharAt()));
  failed = i64(failed + failedIf(checkSliceAndFind()));
  failed = i64(failed + failedIf(checkSliceClamps()));
  failed = i64(failed + failedIf(checkFindFrom()));
  failed = i64(failed + failedIf(checkListLiteralAndForIn()));
  failed = i64(failed + failedIf(checkSplitAndJoin()));
  failed = i64(failed + failedIf(checkTrimAndContains()));
  failed = i64(failed + failedIf(checkStartsAndEnds()));
  failed = i64(failed + failedIf(checkReplaceRepeatIndent()));
  failed = i64(failed + failedIf(checkLines()));
  failed = i64(failed + failedIf(checkFill()));
  failed = i64(failed + failedIf(checkStringBuilder()));
  return failed;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
