declare const process: any;
export {};

import { checkBooleans, checkCallSubject, checkChars, checkConditions, checkConditionsAmongKeys, checkNumbers, checkStrings, checkSums } from "./SwitchTests.js";

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
  failed = i64(failed + failedIf(checkChars()));
  failed = i64(failed + failedIf(checkStrings()));
  failed = i64(failed + failedIf(checkBooleans()));
  failed = i64(failed + failedIf(checkSums()));
  failed = i64(failed + failedIf(checkConditionsAmongKeys()));
  failed = i64(failed + failedIf(checkCallSubject()));
  failed = i64(failed + failedIf(checkConditions()));
  return failed;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
