"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function sumBuf(buf, len) {
  let i = 0n, total = 0.0;
  for (; i < len; i = i64(i + 1n), total = total + buf[Number(i)]) {}
  return total;
}

export { sumBuf };
