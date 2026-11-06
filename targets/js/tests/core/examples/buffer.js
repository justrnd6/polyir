"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function fillToCapacity() {
  const arr = Array.from({ length: 4 }, () => 0n);
  arr[0] = 10n;
  arr[1] = 20n;
  arr[2] = 30n;
  arr[3] = 40n;
  return i64(i64(i64(arr[0] + arr[1]) + arr[2]) + arr[3]);
}

function capacityCheck() {
  const arr = Array.from({ length: 4 }, () => 0n);
  arr[0] = 1n;
  arr[1] = 2n;
  return BigInt(arr.length);
}

function letBoundIndexedFill() {
  const arr = Array.from({ length: 3 }, () => 0n);
  arr[0] = 5n;
  arr[1] = 6n;
  arr[2] = 7n;
  return i64(i64(arr[0] + arr[1]) + arr[2]);
}

function sumViaLoop(n) {
  const arr = Array.from({ length: 10 }, () => 0n);
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    arr[Number(i)] = i;
  }
  let total = 0n;
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    total = i64(total + arr[Number(i)]);
  }
  return total;
}

function stringElements() {
  const names = Array.from({ length: 2 }, () => "");
  names[0] = "first";
  names[1] = "second";
  if (names[0] !== "first") {
    return 0n;
  }
  if (names[1] !== "second") {
    return 0n;
  }
  return 1n;
}

function sumParam(arr) {
  return i64(i64(i64(arr[0] + arr[1]) + arr[2]) + BigInt(arr.length));
}

function viaParam() {
  const arr = Array.from({ length: 3 }, () => 0n);
  arr[0] = 100n;
  arr[1] = 200n;
  arr[2] = 300n;
  return sumParam(arr);
}

export { fillToCapacity, capacityCheck, letBoundIndexedFill, sumViaLoop, stringElements, sumParam, viaParam };
