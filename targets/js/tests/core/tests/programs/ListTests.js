"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function offText(got, want) {
  if (got === want) {
    return 0.0;
  }
  return 1.0;
}

function checkFirstAndLast() {
  const xs = [3n, 1n, 4n];
  const words = ["a", "b"];
  return offText(`${first(xs)} ${last(xs)} ${first(words)}${last(words)}`, "3 4 ab");
}

function checkDrop() {
  const xs = [1n, 2n, 3n];
  const rest = drop(xs, 1n);
  const none = drop(xs, 5n);
  return offText(`${BigInt(rest.length)} ${rest[0]} ${rest[1]} ${BigInt(none.length)}`, "2 2 3 0");
}

function first(xs) {
  return xs[0];
}

function last(xs) {
  return xs[Number(i64(BigInt(xs.length) - 1n))];
}

function drop(xs, n) {
  const out = [];
  for (let i = n; i < BigInt(xs.length); i = i64(i + 1n)) {
    out.push(xs[Number(i)]);
  }
  return out;
}

export { offText, checkFirstAndLast, checkDrop, first, last, drop };
