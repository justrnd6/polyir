// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

const fmaxf = (x: number, y: number): number => Math.fround(Math.max(x, y));

function atomicAddFloat(sums: Array<number>, slot: number, amount: number): number {
  const old: number = sums[slot];
  sums[slot] = Math.fround(old + amount);
  return old;
}

function groupSize(): number {
  return 1;
}

function localIndex(i: number): number {
  return 0;
}

function groupIndex(i: number): number {
  return i;
}

function barrier(): number {
  return 0;
}

function blockSum(i: number, src: Array<number>, n: number, tile: Array<number>, partial: Array<number>): number {
  const lane: number = localIndex(i);
  if (i < n) {
    tile[lane] = src[i];
  } else {
    tile[lane] = 0.0;
  }
  barrier();
  for (let s = (groupSize() / 2) | 0; s > 0; s = (s / 2) | 0) {
    if (lane < s) {
      tile[lane] = Math.fround(tile[lane] + tile[(lane + s) | 0]);
    }
    barrier();
  }
  if (lane === 0) {
    partial[groupIndex(i)] = tile[0];
  }
  return 0;
}

function blockMax(i: number, src: Array<number>, n: number, tile: Array<number>, partial: Array<number>): number {
  const lane: number = localIndex(i);
  if (i < n) {
    tile[lane] = src[i];
  } else {
    tile[lane] = src[0];
  }
  barrier();
  for (let s = (groupSize() / 2) | 0; s > 0; s = (s / 2) | 0) {
    if (lane < s) {
      tile[lane] = fmaxf(tile[lane], tile[(lane + s) | 0]);
    }
    barrier();
  }
  if (lane === 0) {
    partial[groupIndex(i)] = tile[0];
  }
  return 0;
}

function total(i: number, src: Array<number>, n: number, tile: Array<number>, sum: Array<number>): number {
  const lane: number = localIndex(i);
  if (i < n) {
    tile[lane] = src[i];
  } else {
    tile[lane] = 0.0;
  }
  barrier();
  for (let s = (groupSize() / 2) | 0; s > 0; s = (s / 2) | 0) {
    if (lane < s) {
      tile[lane] = Math.fround(tile[lane] + tile[(lane + s) | 0]);
    }
    barrier();
  }
  if (lane === 0) {
    atomicAddFloat(sum, 0, tile[0]);
  }
  return 0;
}

function groupsFor(n: bigint): bigint {
  return i64(n + 255n) / 256n;
}

function launchCount(n: bigint): bigint {
  let launches: bigint = 0n;
  let left: bigint = n;
  for (let k = 0n; k < 10n; k = i64(k + 1n)) {
    if (left > 1n) {
      left = groupsFor(left);
      launches = i64(launches + 1n);
    }
  }
  return launches;
}

export { atomicAddFloat, groupSize, localIndex, groupIndex, barrier, blockSum, blockMax, total, groupsFor, launchCount };
