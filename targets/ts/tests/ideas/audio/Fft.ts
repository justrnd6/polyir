// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

const sqrtf = (x: number): number => Math.fround(Math.sqrt(x));
const sinf = (x: number): number => Math.fround(Math.sin(x));
const cosf = (x: number): number => Math.fround(Math.cos(x));
const logf = (x: number): number => Math.fround(Math.log(x));

function fftStage(t: number, src: Array<number>, p: Array<number>, dst: Array<number>): number {
  const total: number = p[0];
  const n: number = p[1];
  const s: number = p[2];
  const half: number = (total / 2) | 0;
  const batch: number = (t / half) | 0;
  const local: number = t % half;
  const position: number = (local / s) | 0;
  const column: number = local % s;
  const base: number = Math.imul(Math.imul(batch, total), 2);
  const a: number = (base + Number(BigInt.asIntN(32, i64(2n * BigInt((column + Math.imul(s, position)) | 0))))) | 0;
  const b: number = (base + Number(BigInt.asIntN(32, i64(2n * BigInt((column + Math.imul(s, (position + ((n / 2) | 0)) | 0)) | 0))))) | 0;
  const sumRe: number = Math.fround(src[a] + src[b]);
  const sumIm: number = Math.fround(src[(a + 1) | 0] + src[(b + 1) | 0]);
  const diffRe: number = Math.fround(src[a] - src[b]);
  const diffIm: number = Math.fround(src[(a + 1) | 0] - src[(b + 1) | 0]);
  const angle: number = Math.fround((0.0 - 6.2831853) * Math.fround(p[3]) * Math.fround(position) / Math.fround(n));
  const twiddleRe: number = cosf(angle);
  const twiddleIm: number = sinf(angle);
  const first: number = (base + Number(BigInt.asIntN(32, i64(2n * BigInt((column + Math.imul(s, Number(BigInt.asIntN(32, i64(2n * BigInt(position)))))) | 0))))) | 0;
  const second: number = (base + Number(BigInt.asIntN(32, i64(2n * BigInt((column + Math.imul(s, Number(BigInt.asIntN(32, i64(i64(2n * BigInt(position)) + 1n))))) | 0))))) | 0;
  dst[first] = sumRe;
  dst[(first + 1) | 0] = sumIm;
  dst[second] = Math.fround(Math.fround(diffRe * twiddleRe) - Math.fround(diffIm * twiddleIm));
  dst[(second + 1) | 0] = Math.fround(Math.fround(diffRe * twiddleIm) + Math.fround(diffIm * twiddleRe));
  return 0;
}

function fftMagnitude(k: number, data: Array<number>, p: Array<number>): number {
  const total: number = p[0];
  const re: number = data[Number(i64(2n * BigInt(k)))];
  const im: number = data[Number(i64(i64(2n * BigInt(k)) + 1n))];
  return Math.fround(2.0 * sqrtf(Math.fround(Math.fround(re * re) + Math.fround(im * im))) / Math.fround(total));
}

function fftFrames(t: number, signal: Array<number>, p: Array<number>, dst: Array<number>): number {
  const total: number = p[0];
  const frame: number = (t / total) | 0;
  const k: number = t % total;
  const window: number = Math.fround(0.5 - 0.5 * cosf(Math.fround(6.2831853 * Math.fround(k) / Math.fround((total - 1) | 0))));
  dst[Number(i64(2n * BigInt(t)))] = Math.fround(signal[(Math.imul(frame, p[1]) + k) | 0] * window);
  dst[Number(i64(i64(2n * BigInt(t)) + 1n))] = 0.0;
  return 0;
}

function spectrogramPixel(i: number, data: Array<number>, p: Array<number>): number {
  const total: number = p[0];
  const frames: number = p[1];
  const rows: number = p[2];
  const x: number = i % frames;
  const bin: number = (((rows - 1) | 0) - ((i / frames) | 0)) | 0;
  const at: number = (Math.imul(Math.imul(x, total), 2) + Number(BigInt.asIntN(32, i64(2n * BigInt(bin))))) | 0;
  const re: number = data[at];
  const im: number = data[(at + 1) | 0];
  const magnitude: number = Math.fround(4.0 * sqrtf(Math.fround(Math.fround(re * re) + Math.fround(im * im))) / Math.fround(total));
  let level: number = Math.fround((20.0 * logf(Math.fround(magnitude + 0.0000001)) / 2.3025851 + 70.0) / 70.0);
  if (level < 0.0) {
    level = 0.0;
  }
  if (level > 1.0) {
    level = 1.0;
  }
  return level;
}

function complexMultiply(k: number, a: Array<number>, b: Array<number>, out: Array<number>): number {
  out[Number(i64(2n * BigInt(k)))] = Math.fround(Math.fround(a[Number(i64(2n * BigInt(k)))] * b[Number(i64(2n * BigInt(k)))]) - Math.fround(a[Number(i64(i64(2n * BigInt(k)) + 1n))] * b[Number(i64(i64(2n * BigInt(k)) + 1n))]));
  out[Number(i64(i64(2n * BigInt(k)) + 1n))] = Math.fround(Math.fround(a[Number(i64(2n * BigInt(k)))] * b[Number(i64(i64(2n * BigInt(k)) + 1n))]) + Math.fround(a[Number(i64(i64(2n * BigInt(k)) + 1n))] * b[Number(i64(2n * BigInt(k)))]));
  return 0;
}

function realScaled(i: number, data: Array<number>, p: Array<number>): number {
  return Math.fround(data[Number(i64(2n * BigInt(i)))] / p[0]);
}

export { fftStage, fftMagnitude, fftFrames, spectrogramPixel, complexMultiply, realScaled };
