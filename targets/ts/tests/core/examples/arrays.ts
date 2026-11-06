// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function histTest(): bigint {
  const hist: Array<bigint> = Array.from({ length: 4 }, () => 0n);
  hist[0] = 10n;
  hist[1] = 20n;
  hist[2] = 30n;
  hist[3] = 40n;
  return i64(hist[1] + hist[2]);
}

function lenCheck(): bigint {
  const hist: Array<bigint> = Array.from({ length: 4 }, () => 0n);
  return BigInt(hist.length);
}

function sumFixed(arr: Array<bigint>): bigint {
  return i64(i64(i64(arr[0] + arr[1]) + arr[2]) + arr[3]);
}

function lenOnParam(arr: Array<bigint>): bigint {
  return BigInt(arr.length);
}

function callWithLocal(): bigint {
  const hist: Array<bigint> = Array.from({ length: 4 }, () => 0n);
  hist[0] = 1n;
  hist[1] = 2n;
  hist[2] = 3n;
  hist[3] = 4n;
  return i64(i64(sumFixed(hist) * 100n) + lenOnParam(hist));
}

export { histTest, lenCheck, sumFixed, lenOnParam, callWithLocal };
