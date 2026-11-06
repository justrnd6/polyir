"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function square(x) {
  return i64(x * x);
}

function sub(a, b) {
  return i64(a - b);
}

function equivalenceCheck(x) {
  return i64(square(x) - square(x));
}

function orderingCheck(a, b) {
  return i64(sub(a, b) - i64(a - b));
}

function pushViaMethodPersists() {
  const arr = [];
  arr.push(10n);
  arr.push(20n);
  return BigInt(arr.length);
}

function pushViaMethodSum() {
  const arr = [];
  arr.push(1n);
  arr.push(2n);
  arr.push(3n);
  return i64(i64(arr[0] + arr[1]) + arr[2]);
}

export { square, sub, equivalenceCheck, orderingCheck, pushViaMethodPersists, pushViaMethodSum };
