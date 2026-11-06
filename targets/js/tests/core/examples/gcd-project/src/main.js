"use strict";

import { gcd, lcm } from "./euclid.js";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function main() {
  return i64(i64(gcd(48n, 18n) * 10n) + lcm(4n, 6n));
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
