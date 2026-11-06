const floorf = (x: number): number => Math.fround(Math.floor(x));
const sinf = (x: number): number => Math.fround(Math.sin(x));
const cosf = (x: number): number => Math.fround(Math.cos(x));

function atomicAdd(counts: Array<number>, slot: number, amount: number): number {
  const old: number = counts[slot];
  counts[slot] = (old + amount) | 0;
  return old;
}

function reverseCopy(i: number, src: Array<number>, n: number, dst: Array<number>): number {
  dst[(((n - 1) | 0) - i) | 0] = src[i];
  return 0;
}

function histogram(i: number, data: Array<number>, bins: number, counts: Array<number>): number {
  const bin: number = data[i] % bins;
  atomicAdd(counts, bin, 1);
  return 0;
}

function histogramRacy(i: number, data: Array<number>, bins: number, counts: Array<number>): number {
  const bin: number = data[i] % bins;
  counts[bin] = (counts[bin] + 1) | 0;
  return 0;
}

function splat(i: number, p: Array<number>, cells: Array<number>): number {
  const w: number = p[0] | 0;
  const arm: number = i % 2;
  const t: number = Math.fround(Math.fround(((i / 2) | 0) % 20000) / 20000.0);
  const angle: number = Math.fround(Math.fround(t * 9.0) + Math.fround(Math.fround(arm) * 3.14159265));
  const radius: number = Math.fround(0.08 + 0.85 * t);
  const h1: number = ((Math.imul(i, 75) + 74) | 0) % 65537;
  const h2: number = ((Math.imul(h1, 75) + 74) | 0) % 65537;
  const spread: number = Math.fround(0.015 + 0.09 * t);
  const x: number = Math.fround(Math.fround(radius * cosf(angle)) + Math.fround(Math.fround(Math.fround(Math.fround(Math.fround(h1) / 65537.0) - 0.5) * spread) * 2.0));
  const y: number = Math.fround(Math.fround(radius * sinf(angle)) + Math.fround(Math.fround(Math.fround(Math.fround(Math.fround(h2) / 65537.0) - 0.5) * spread) * 2.0));
  const px: number = floorf(Math.fround(Math.fround(Math.fround(x * 0.5) + 0.5) * Math.fround(w))) | 0;
  const py: number = floorf(Math.fround(Math.fround(Math.fround(y * 0.5) + 0.5) * Math.fround(w))) | 0;
  if (px >= 0 && px < w && py >= 0 && py < w) {
    atomicAdd(cells, (Math.imul(py, w) + px) | 0, 1);
  }
  return 0;
}

export { atomicAdd, reverseCopy, histogram, histogramRacy, splat };
