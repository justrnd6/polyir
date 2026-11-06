"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

const fminf = (x, y) => Math.fround(Math.min(x, y));
const fmaxf = (x, y) => Math.fround(Math.max(x, y));
const floorf = (x) => Math.fround(Math.floor(x));
const ceilf = (x) => Math.fround(Math.ceil(x));

function atomicMin(values, slot, candidate) {
  const old = values[slot];
  if (candidate < old) {
    values[slot] = candidate;
  }
  return old;
}

function emptyKey() {
  return 2147483647;
}

function rasterize(t, tris, width, height, depth) {
  const base = Math.imul(t, 10);
  const x0 = tris[base];
  const y0 = tris[(base + 1) | 0];
  const z0 = tris[(base + 2) | 0];
  const x1 = tris[(base + 3) | 0];
  const y1 = tris[(base + 4) | 0];
  const z1 = tris[(base + 5) | 0];
  const x2 = tris[(base + 6) | 0];
  const y2 = tris[(base + 7) | 0];
  const z2 = tris[(base + 8) | 0];
  const area = Math.fround(Math.fround(Math.fround(x1 - x0) * Math.fround(y2 - y0)) - Math.fround(Math.fround(x2 - x0) * Math.fround(y1 - y0)));
  const left = fmaxf(0.0, floorf(fminf(x0, fminf(x1, x2)))) | 0;
  const right = fminf(Math.fround(Math.fround(width) - 1.0), ceilf(fmaxf(x0, fmaxf(x1, x2)))) | 0;
  const top = fmaxf(0.0, floorf(fminf(y0, fminf(y1, y2)))) | 0;
  const bottom = fminf(Math.fround(Math.fround(height) - 1.0), ceilf(fmaxf(y0, fmaxf(y1, y2)))) | 0;
  for (let y = top; y <= bottom; y = (y + 1) | 0) {
    for (let x = left; x <= right; x = (x + 1) | 0) {
      const px = Math.fround(Math.fround(x) + 0.5);
      const py = Math.fround(Math.fround(y) + 0.5);
      const w0 = Math.fround(Math.fround(Math.fround(Math.fround(x1 - px) * Math.fround(y2 - py)) - Math.fround(Math.fround(x2 - px) * Math.fround(y1 - py))) / area);
      const w1 = Math.fround(Math.fround(Math.fround(Math.fround(x2 - px) * Math.fround(y0 - py)) - Math.fround(Math.fround(x0 - px) * Math.fround(y2 - py))) / area);
      const w2 = Math.fround(1.0 - w0 - w1);
      if (w0 >= 0.0 && w1 >= 0.0 && w2 >= 0.0) {
        const z = Math.fround(Math.fround(Math.fround(w0 * z0) + Math.fround(w1 * z1)) + Math.fround(w2 * z2));
        atomicMin(depth, (Math.imul(y, width) + x) | 0, (Math.imul(Math.fround(z * 1048575.0) | 0, 512) + t) | 0);
      }
    }
  }
  return 0;
}

function shade(p, depth, tris) {
  const key = depth[p];
  if (key === 2147483647) {
    return 0.06;
  }
  const id = key % 512;
  const z = Math.fround(Math.fround((key / 512) | 0) / 1048575.0);
  return Math.fround(tris[(Math.imul(id, 10) + 9) | 0] * Math.fround(1.0 - 0.45 * z));
}

function makeScene(tris, count, width, height) {
  let seed = 12345n;
  for (let t = 0n; t < count; t = i64(t + 1n)) {
    const cx = Math.fround(nextRandom(seed) * Math.fround(Number(width)));
    seed = nextSeed(seed);
    const cy = Math.fround(nextRandom(seed) * Math.fround(Number(height)));
    seed = nextSeed(seed);
    const size = Math.fround(30.0 + Math.fround(nextRandom(seed) * 110.0));
    seed = nextSeed(seed);
    for (let corner = 0n; corner < 3n; corner = i64(corner + 1n)) {
      tris[Number(i64(i64(t * 10n) + i64(corner * 3n)))] = Math.fround(cx + Math.fround(Math.fround(Math.fround(nextRandom(seed) - 0.5) * 2.0) * size));
      seed = nextSeed(seed);
      tris[Number(i64(i64(i64(t * 10n) + i64(corner * 3n)) + 1n))] = Math.fround(cy + Math.fround(Math.fround(Math.fround(nextRandom(seed) - 0.5) * 2.0) * size));
      seed = nextSeed(seed);
      tris[Number(i64(i64(i64(t * 10n) + i64(corner * 3n)) + 2n))] = nextRandom(seed);
      seed = nextSeed(seed);
    }
    tris[Number(i64(i64(t * 10n) + 9n))] = Math.fround(0.25 + 0.75 * nextRandom(seed));
    seed = nextSeed(seed);
  }
  return 0n;
}

function nextSeed(seed) {
  return i64(i64(seed * 1103515245n) + 12345n) % 2147483648n;
}

function nextRandom(seed) {
  return Math.fround(Math.fround(Number(seed / 65536n % 32768n)) / 32768.0);
}

export { atomicMin, emptyKey, rasterize, shade, makeScene, nextSeed, nextRandom };
