// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

let counter: bigint = 0n;

function bump(): bigint {
  counter = i64(counter + 1n);
  return counter;
}

export { bump };
