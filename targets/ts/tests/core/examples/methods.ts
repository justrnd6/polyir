// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function square(x: bigint): bigint {
  return i64(x * x);
}

function sub(a: bigint, b: bigint): bigint {
  return i64(a - b);
}

function equivalenceCheck(x: bigint): bigint {
  return i64(square(x) - square(x));
}

function orderingCheck(a: bigint, b: bigint): bigint {
  return i64(sub(a, b) - i64(a - b));
}

function pushViaMethodPersists(): bigint {
  const arr: Array<bigint> = [];
  arr.push(10n);
  arr.push(20n);
  return BigInt(arr.length);
}

function pushViaMethodSum(): bigint {
  const arr: Array<bigint> = [];
  arr.push(1n);
  arr.push(2n);
  arr.push(3n);
  return i64(i64(arr[0] + arr[1]) + arr[2]);
}

export { square, sub, equivalenceCheck, orderingCheck, pushViaMethodPersists, pushViaMethodSum };
