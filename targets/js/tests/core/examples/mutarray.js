"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

// A push used as a value gives the list, as in polyir (JavaScript's push gives the length).
function pushed(items, item) {
  items.push(item);
  return items;
}

function sumPushed(a, b, c) {
  const arr = [];
  arr.push(a);
  arr.push(b);
  arr.push(c);
  arr.push(i64(i64(a + b) + c));
  return i64(i64(i64(arr[0] + arr[1]) + arr[2]) + arr[3]);
}

function lenCheck(a, b, c) {
  const arr = [];
  arr.push(a);
  arr.push(b);
  arr.push(c);
  return BigInt(arr.length);
}

function sumSquaresUpTo(n) {
  const arr = [];
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    arr.push(i64(i * i));
  }
  let total = 0n;
  for (let i = 0n; i < BigInt(arr.length); i = i64(i + 1n)) {
    total = i64(total + arr[Number(i)]);
  }
  return total;
}

function sumEvensBelow(n) {
  const arr = [];
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    if (i % 2n === 0n) {
      arr.push(i);
    }
  }
  let total = 0n;
  for (let i = 0n; i < BigInt(arr.length); i = i64(i + 1n)) {
    total = i64(total + arr[Number(i)]);
  }
  return total;
}

function fillSquares(arr, count) {
  for (let i = 0n; i < count; i = i64(i + 1n)) {
    arr.push(i64(i * i));
  }
  return count;
}

function squaresThroughAHelper(count) {
  const arr = [];
  fillSquares(arr, count);
  return i64(i64(BigInt(arr.length) * 1000n) + arr[Number(i64(count - 1n))]);
}

function pushGivesTheArrayBack() {
  let arr = [];
  arr = pushed(arr, 10n);
  arr = pushed(arr, 20n);
  return i64(arr[0] + arr[1]);
}

function sumEvensBelowExplicit(n) {
  let label = 0n;
  let i = 0n;
  let arr = [];
  machine: for (;;) {
    switch (label) {
      case 0n: {
        const empty = [];
        i = 0n;
        arr = empty;
        label = 1n;
        continue machine;
      }
      case 1n: {
        if (i < n) {
          label = 2n;
          continue machine;
        } else {
          label = 5n;
          continue machine;
        }
      }
      case 2n: {
        if (i % 2n === 0n) {
          label = 3n;
          continue machine;
        } else {
          label = 4n;
          continue machine;
        }
      }
      case 3n: {
        let next_i = i64(i + 1n);
        let next_arr = pushed(arr, i);
        i = next_i;
        arr = next_arr;
        label = 1n;
        continue machine;
      }
      case 4n: {
        i = i64(i + 1n);
        label = 1n;
        continue machine;
      }
      case 5n: {
        let i = 0n, total = 0n;
        for (; i < BigInt(arr.length); i = i64(i + 1n), total = i64(total + arr[Number(i)])) {}
        return total;
      }
    }
  }
}

export { sumPushed, lenCheck, sumSquaresUpTo, sumEvensBelow, fillSquares, squaresThroughAHelper, pushGivesTheArrayBack, sumEvensBelowExplicit };
