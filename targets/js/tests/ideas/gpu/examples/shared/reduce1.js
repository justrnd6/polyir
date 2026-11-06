"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

const fmaxf = (x, y) => Math.fround(Math.max(x, y));

function atomicAddFloat(sums, slot, amount) {
  const old = sums[slot];
  sums[slot] = Math.fround(old + amount);
  return old;
}

function groupSize() {
  return 1;
}

function localIndex(i) {
  return 0;
}

function groupIndex(i) {
  return i;
}

function barrier() {
  return 0;
}

function blockSum(i, src, n, tile, partial) {
  const lane = localIndex(i);
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

function blockMax(i, src, n, tile, partial) {
  const lane = localIndex(i);
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

function total(i, src, n, tile, sum) {
  const lane = localIndex(i);
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

function groupsFor(n) {
  return i64(n + 255n) / 256n;
}

function launchCount(n) {
  let launches = 0n;
  let left = n;
  for (let k = 0n; k < 10n; k = i64(k + 1n)) {
    if (left > 1n) {
      left = groupsFor(left);
      launches = i64(launches + 1n);
    }
  }
  return launches;
}

export { atomicAddFloat, groupSize, localIndex, groupIndex, barrier, blockSum, blockMax, total, groupsFor, launchCount };
