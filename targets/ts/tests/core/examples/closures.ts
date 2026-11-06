// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function counterTwice(): bigint {
  let count: bigint = 0n;
  const inc: () => bigint = (): bigint => {
    count = i64(count + 1n);
    return count;
  };
  const a: bigint = inc();
  const b: bigint = inc();
  return i64(i64(a * 100n) + b);
}

function makeMultiplier(factor: bigint): (...args: [bigint]) => bigint {
  const mul: (...args: [bigint]) => bigint = (x: bigint) => i64(x * factor);
  return mul;
}

function tripleOf(x: bigint): bigint {
  const triple: (...args: [bigint]) => bigint = makeMultiplier(3n);
  return triple(x);
}

function sharedCounter(): bigint {
  let shared: bigint = 0n;
  const bumpBy1: () => bigint = (): bigint => {
    shared = i64(shared + 1n);
    return shared;
  };
  const bumpBy10: () => bigint = (): bigint => {
    shared = i64(shared + 10n);
    return shared;
  };
  const a: bigint = bumpBy1();
  const b: bigint = bumpBy10();
  const c: bigint = bumpBy1();
  return i64(i64(i64(a * 10000n) + i64(b * 100n)) + c);
}

export { counterTwice, makeMultiplier, tripleOf, sharedCounter };
