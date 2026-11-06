const sinf = (x: number): number => Math.fround(Math.sin(x));

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

function blend7(a: number, b: number, c: number, d: number, e: number, f: number, g: number, k: number): number {
  return Math.fround(Math.fround(k * Math.fround(Math.fround(a + Math.fround(2.0 * b)) + Math.fround(3.0 * c))) + Math.fround(Math.fround(d - e) * Math.fround(f + g)));
}

function plasma(x: number, y: number, width: number, height: number, phase: number): number {
  const u: number = Math.fround(Math.fround(x) / Math.fround(width));
  const v: number = Math.fround(Math.fround(y) / Math.fround(height));
  const a: number = sinf(Math.fround(10.0 * u + phase));
  const b: number = sinf(Math.fround(8.0 * v - phase));
  const c: number = sinf(Math.fround(6.0 * Math.fround(u + v) + 2.0 * phase));
  return Math.fround(0.5 + 0.1667 * Math.fround(Math.fround(a + b) + c));
}

function vignette(pixel: number, x: number, y: number, width: number, height: number): number {
  const dx: number = Math.fround(Math.fround(Math.fround(x) / Math.fround(width)) - 0.5);
  const dy: number = Math.fround(Math.fround(Math.fround(y) / Math.fround(height)) - 0.5);
  const edge: number = Math.fround(1.0 - 1.6 * Math.fround(Math.fround(dx * dx) + Math.fround(dy * dy)));
  return Math.fround(pixel * edge);
}

function tally(i: number, counts: Array<number>, sums: Array<number>, lows: Array<number>, highs: Array<number>): number {
  const value: number = Math.imul(i, 7919) % 10007;
  atomicAdd(counts, i % 4, 1);
  atomicAddFloat(sums, i % 4, 0.5);
  atomicMin(lows, 0, value);
  atomicMax(highs, 0, value);
  return 0;
}

function reported(i: number): number {
  return Math.imul(i, 7919) % 10007;
}

function nearly(a: number, b: number, tolerance: number): bigint {
  let gap: number = Math.fround(a - b);
  if (gap < 0.0) {
    gap = Math.fround(0.0 - gap);
  }
  if (gap <= tolerance) {
    return 1n;
  }
  return 0n;
}

export { atomicAdd, atomicAddFloat, atomicMin, atomicMax, blend7, plasma, vignette, tally, reported, nearly };
