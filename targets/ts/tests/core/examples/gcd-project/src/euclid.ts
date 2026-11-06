// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function gcd(a: bigint, b: bigint): bigint {
  let x: bigint = a;
  let y: bigint = b;
  while (y !== 0n) {
    const r: bigint = x % y;
    x = y;
    y = r;
  }
  return x;
}

function lcm(a: bigint, b: bigint): bigint {
  return i64(a / gcd(a, b) * b);
}

export { gcd, lcm };
