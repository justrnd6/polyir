// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function offText(got: string, want: string): number {
  if (got === want) {
    return 0.0;
  }
  return 1.0;
}

function checkFirstAndLast(): number {
  const xs: Array<bigint> = [3n, 1n, 4n];
  const words: Array<string> = ["a", "b"];
  return offText(`${first(xs)} ${last(xs)} ${first(words)}${last(words)}`, "3 4 ab");
}

function checkDrop(): number {
  const xs: Array<bigint> = [1n, 2n, 3n];
  const rest: Array<bigint> = drop(xs, 1n);
  const none: Array<bigint> = drop(xs, 5n);
  return offText(`${BigInt(rest.length)} ${rest[0]} ${rest[1]} ${BigInt(none.length)}`, "2 2 3 0");
}

function first<T>(xs: Array<T>): T {
  return xs[0];
}

function last<T>(xs: Array<T>): T {
  return xs[Number(i64(BigInt(xs.length) - 1n))];
}

function drop<T>(xs: Array<T>, n: bigint): Array<T> {
  const out: Array<T> = [];
  for (let i = n; i < BigInt(xs.length); i = i64(i + 1n)) {
    out.push(xs[Number(i)]);
  }
  return out;
}

export { offText, checkFirstAndLast, checkDrop, first, last, drop };
