declare const process: any;
export {};

import { checkBooleans, checkComparisons, checkConditions, checkEithers, checkGuardsOnTheSubject, checkListGuards, checkLists, checkNested, checkNumbers, checkOptionals, checkPayloadPatterns, checkStatements, checkStrings, checkVariants } from "./MatchTests.js";

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function failedIf(off: number): bigint {
  if (off > 0.001 || off < 0.0 - 0.001) {
    return 1n;
  }
  return 0n;
}

function main(): bigint {
  let failed: bigint = 0n;
  failed = i64(failed + failedIf(checkNumbers()));
  failed = i64(failed + failedIf(checkComparisons()));
  failed = i64(failed + failedIf(checkGuardsOnTheSubject()));
  failed = i64(failed + failedIf(checkStrings()));
  failed = i64(failed + failedIf(checkConditions()));
  failed = i64(failed + failedIf(checkBooleans()));
  failed = i64(failed + failedIf(checkOptionals()));
  failed = i64(failed + failedIf(checkEithers()));
  failed = i64(failed + failedIf(checkVariants()));
  failed = i64(failed + failedIf(checkPayloadPatterns()));
  failed = i64(failed + failedIf(checkLists()));
  failed = i64(failed + failedIf(checkListGuards()));
  failed = i64(failed + failedIf(checkStatements()));
  failed = i64(failed + failedIf(checkNested()));
  return failed;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
