function atomicAdd(counts: Array<number>, slot: number, amount: number): number {
  const old: number = counts[slot];
  counts[slot] = (old + amount) | 0;
  return old;
}

function atomicAddFloat(sums: Array<number>, slot: number, amount: number): number {
  const old: number = sums[slot];
  sums[slot] = Math.fround(old + amount);
  return old;
}

function atomicMin(values: Array<number>, slot: number, candidate: number): number {
  const old: number = values[slot];
  if (candidate < old) {
    values[slot] = candidate;
  }
  return old;
}

function atomicMax(values: Array<number>, slot: number, candidate: number): number {
  const old: number = values[slot];
  if (candidate > old) {
    values[slot] = candidate;
  }
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

export { atomicAdd, atomicAddFloat, atomicMin, atomicMax, groupSize, localIndex, groupIndex, barrier };
