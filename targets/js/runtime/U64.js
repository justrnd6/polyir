// An unsigned 64-bit integer is a BigInt, brought back into 0 .. 2^64 - 1 after an operation that can leave them.
function u64(value) {
  return BigInt.asUintN(64, value);
}
