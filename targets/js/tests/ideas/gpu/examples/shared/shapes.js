"use strict";

const sinf = (x) => Math.fround(Math.sin(x));

function atomicAdd(counts, slot, amount) {
  const old = counts[slot];
  counts[slot] = (old + amount) | 0;
  return old;
}

function atomicAddFloat(sums, slot, amount) {
  const old = sums[slot];
  sums[slot] = Math.fround(old + amount);
  return old;
}

function atomicMin(values, slot, candidate) {
  const old = values[slot];
  if (candidate < old) {
    values[slot] = candidate;
  }
  return old;
}

function atomicMax(values, slot, candidate) {
  const old = values[slot];
  if (candidate > old) {
    values[slot] = candidate;
  }
  return old;
}

function blend7(a, b, c, d, e, f, g, k) {
  return Math.fround(Math.fround(k * Math.fround(Math.fround(a + Math.fround(2.0 * b)) + Math.fround(3.0 * c))) + Math.fround(Math.fround(d - e) * Math.fround(f + g)));
}

function plasma(x, y, width, height, phase) {
  const u = Math.fround(Math.fround(x) / Math.fround(width));
  const v = Math.fround(Math.fround(y) / Math.fround(height));
  const a = sinf(Math.fround(10.0 * u + phase));
  const b = sinf(Math.fround(8.0 * v - phase));
  const c = sinf(Math.fround(6.0 * Math.fround(u + v) + 2.0 * phase));
  return Math.fround(0.5 + 0.1667 * Math.fround(Math.fround(a + b) + c));
}

function vignette(pixel, x, y, width, height) {
  const dx = Math.fround(Math.fround(Math.fround(x) / Math.fround(width)) - 0.5);
  const dy = Math.fround(Math.fround(Math.fround(y) / Math.fround(height)) - 0.5);
  const edge = Math.fround(1.0 - 1.6 * Math.fround(Math.fround(dx * dx) + Math.fround(dy * dy)));
  return Math.fround(pixel * edge);
}

function tally(i, counts, sums, lows, highs) {
  const value = Math.imul(i, 7919) % 10007;
  atomicAdd(counts, i % 4, 1);
  atomicAddFloat(sums, i % 4, 0.5);
  atomicMin(lows, 0, value);
  atomicMax(highs, 0, value);
  return 0;
}

function reported(i) {
  return Math.imul(i, 7919) % 10007;
}

function nearly(a, b, tolerance) {
  let gap = Math.fround(a - b);
  if (gap < 0.0) {
    gap = Math.fround(0.0 - gap);
  }
  if (gap <= tolerance) {
    return 1n;
  }
  return 0n;
}

export { atomicAdd, atomicAddFloat, atomicMin, atomicMax, blend7, plasma, vignette, tally, reported, nearly };
