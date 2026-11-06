"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function counterTwice() {
  let count = 0n;
  const inc = () => {
    count = i64(count + 1n);
    return count;
  };
  const a = inc();
  const b = inc();
  return i64(i64(a * 100n) + b);
}

function makeMultiplier(factor) {
  const mul = (x) => i64(x * factor);
  return mul;
}

function tripleOf(x) {
  const triple = makeMultiplier(3n);
  return triple(x);
}

function sharedCounter() {
  let shared = 0n;
  const bumpBy1 = () => {
    shared = i64(shared + 1n);
    return shared;
  };
  const bumpBy10 = () => {
    shared = i64(shared + 10n);
    return shared;
  };
  const a = bumpBy1();
  const b = bumpBy10();
  const c = bumpBy1();
  return i64(i64(i64(a * 10000n) + i64(b * 100n)) + c);
}

export { counterTwice, makeMultiplier, tripleOf, sharedCounter };
