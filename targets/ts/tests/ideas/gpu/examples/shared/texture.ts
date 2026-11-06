// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

const floorf = (x: number): number => Math.fround(Math.floor(x));

function wrap(n: number, size: number, mode: number): number {
  if (mode === 0) {
    if (n < 0) {
      return 0;
    }
    if (n > ((size - 1) | 0)) {
      return (size - 1) | 0;
    }
    return n;
  }
  if (mode === 1) {
    return ((n % size + size) | 0) % size;
  }
  const period: number = Math.imul(size, 2);
  const m: number = ((n % period + period) | 0) % period;
  if (m >= size) {
    return (((period - 1) | 0) - m) | 0;
  }
  return m;
}

function texel(tex: Array<number>, offset: number, size: number, x: number, y: number, mode: number): number {
  return tex[(((offset + Math.imul(wrap(y, size, mode), size)) | 0) + wrap(x, size, mode)) | 0];
}

function nearest(tex: Array<number>, offset: number, size: number, u: number, v: number, mode: number): number {
  const x: number = floorf(Math.fround(u * Math.fround(size))) | 0;
  const y: number = floorf(Math.fround(v * Math.fround(size))) | 0;
  return texel(tex, offset, size, x, y, mode);
}

function lerp(a: number, b: number, t: number): number {
  return Math.fround(a + Math.fround(Math.fround(b - a) * t));
}

function bilinear(tex: Array<number>, offset: number, size: number, u: number, v: number, mode: number): number {
  const sx: number = Math.fround(Math.fround(u * Math.fround(size)) - 0.5);
  const sy: number = Math.fround(Math.fround(v * Math.fround(size)) - 0.5);
  const x0: number = floorf(sx) | 0;
  const y0: number = floorf(sy) | 0;
  const fx: number = Math.fround(sx - Math.fround(x0));
  const fy: number = Math.fround(sy - Math.fround(y0));
  const top: number = lerp(texel(tex, offset, size, x0, y0, mode), texel(tex, offset, size, (x0 + 1) | 0, y0, mode), fx);
  const bottom: number = lerp(texel(tex, offset, size, x0, (y0 + 1) | 0, mode), texel(tex, offset, size, (x0 + 1) | 0, (y0 + 1) | 0, mode), fx);
  return lerp(top, bottom, fy);
}

function levelOffset(base: number, level: number): number {
  let offset: number = 0;
  let size: number = base;
  for (let k = 0n; k < BigInt(level); k = i64(k + 1n)) {
    offset = (offset + Math.imul(size, size)) | 0;
    size = (size / 2) | 0;
  }
  return offset;
}

function levelSize(base: number, level: number): number {
  let size: number = base;
  for (let k = 0n; k < BigInt(level); k = i64(k + 1n)) {
    size = (size / 2) | 0;
  }
  return size;
}

function mipSample(tex: Array<number>, base: number, levels: number, u: number, v: number, level: number, mode: number): number {
  let lo: number = floorf(level) | 0;
  if (lo < 0) {
    lo = 0;
  }
  if (lo > ((levels - 1) | 0)) {
    lo = (levels - 1) | 0;
  }
  let hi: number = (lo + 1) | 0;
  if (hi > ((levels - 1) | 0)) {
    hi = (levels - 1) | 0;
  }
  let amount: number = Math.fround(level - Math.fround(lo));
  if (amount < 0.0) {
    amount = 0.0;
  }
  if (amount > 1.0) {
    amount = 1.0;
  }
  const a: number = bilinear(tex, levelOffset(base, lo), levelSize(base, lo), u, v, mode);
  const b: number = bilinear(tex, levelOffset(base, hi), levelSize(base, hi), u, v, mode);
  return lerp(a, b, amount);
}

function pyramidSize(base: bigint): bigint {
  let total: bigint = 0n;
  let size: bigint = base;
  for (let k = 0n; k < 16n; k = i64(k + 1n)) {
    if (size >= 1n) {
      total = i64(total + i64(size * size));
      size = size / 2n;
    }
  }
  return total;
}

function fillBase(tex: Array<number>, base: bigint): bigint {
  for (let y = 0n; y < base; y = i64(y + 1n)) {
    for (let x = 0n; x < base; x = i64(x + 1n)) {
      let level: number = i64(x / 4n + y / 4n) % 2n === 1n ? 0.45 : 0.85;
      if (x % 4n === 0n || y % 4n === 0n) {
        level = Math.fround(level * 0.6);
      }
      tex[Number(i64(i64(y * base) + x))] = level;
    }
  }
  return 0n;
}

function buildPyramid(tex: Array<number>, base: bigint): bigint {
  fillBase(tex, base);
  let from: bigint = 0n;
  let size: bigint = base;
  for (let level = 0n; level < 16n; level = i64(level + 1n)) {
    if (size > 1n) {
      shrink(tex, from, size);
      from = i64(from + i64(size * size));
      size = size / 2n;
    }
  }
  return 0n;
}

function shrink(tex: Array<number>, from: bigint, size: bigint): bigint {
  const next: bigint = i64(from + i64(size * size));
  const half: bigint = size / 2n;
  for (let y = 0n; y < half; y = i64(y + 1n)) {
    for (let x = 0n; x < half; x = i64(x + 1n)) {
      const a: number = tex[Number(i64(i64(from + i64(i64(2n * y) * size)) + i64(2n * x)))];
      const b: number = tex[Number(i64(i64(i64(from + i64(i64(2n * y) * size)) + i64(2n * x)) + 1n))];
      const c: number = tex[Number(i64(i64(from + i64(i64(i64(2n * y) + 1n) * size)) + i64(2n * x)))];
      const d: number = tex[Number(i64(i64(i64(from + i64(i64(i64(2n * y) + 1n) * size)) + i64(2n * x)) + 1n))];
      tex[Number(i64(i64(next + i64(y * half)) + x))] = Math.fround(Math.fround(Math.fround(Math.fround(a + b) + c) + d) * 0.25);
    }
  }
  return 0n;
}

export { wrap, texel, nearest, lerp, bilinear, levelOffset, levelSize, mipSample, pyramidSize, fillBase, buildPyramid, shrink };
