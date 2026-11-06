"use strict";

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

export { atomicAdd, atomicAddFloat, atomicMin, atomicMax, groupSize, localIndex, groupIndex, barrier };
