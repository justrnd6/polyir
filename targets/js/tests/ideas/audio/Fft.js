"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

const sqrtf = (x) => Math.fround(Math.sqrt(x));
const sinf = (x) => Math.fround(Math.sin(x));
const cosf = (x) => Math.fround(Math.cos(x));
const logf = (x) => Math.fround(Math.log(x));

function fftStage(t, src, p, dst) {
  const total = p[0];
  const n = p[1];
  const s = p[2];
  const half = (total / 2) | 0;
  const batch = (t / half) | 0;
  const local = t % half;
  const position = (local / s) | 0;
  const column = local % s;
  const base = Math.imul(Math.imul(batch, total), 2);
  const a = (base + Number(BigInt.asIntN(32, i64(2n * BigInt((column + Math.imul(s, position)) | 0))))) | 0;
  const b = (base + Number(BigInt.asIntN(32, i64(2n * BigInt((column + Math.imul(s, (position + ((n / 2) | 0)) | 0)) | 0))))) | 0;
  const sumRe = Math.fround(src[a] + src[b]);
  const sumIm = Math.fround(src[(a + 1) | 0] + src[(b + 1) | 0]);
  const diffRe = Math.fround(src[a] - src[b]);
  const diffIm = Math.fround(src[(a + 1) | 0] - src[(b + 1) | 0]);
  const angle = Math.fround((0.0 - 6.2831853) * Math.fround(p[3]) * Math.fround(position) / Math.fround(n));
  const twiddleRe = cosf(angle);
  const twiddleIm = sinf(angle);
  const first = (base + Number(BigInt.asIntN(32, i64(2n * BigInt((column + Math.imul(s, Number(BigInt.asIntN(32, i64(2n * BigInt(position)))))) | 0))))) | 0;
  const second = (base + Number(BigInt.asIntN(32, i64(2n * BigInt((column + Math.imul(s, Number(BigInt.asIntN(32, i64(i64(2n * BigInt(position)) + 1n))))) | 0))))) | 0;
  dst[first] = sumRe;
  dst[(first + 1) | 0] = sumIm;
  dst[second] = Math.fround(Math.fround(diffRe * twiddleRe) - Math.fround(diffIm * twiddleIm));
  dst[(second + 1) | 0] = Math.fround(Math.fround(diffRe * twiddleIm) + Math.fround(diffIm * twiddleRe));
  return 0;
}

function fftMagnitude(k, data, p) {
  const total = p[0];
  const re = data[Number(i64(2n * BigInt(k)))];
  const im = data[Number(i64(i64(2n * BigInt(k)) + 1n))];
  return Math.fround(2.0 * sqrtf(Math.fround(Math.fround(re * re) + Math.fround(im * im))) / Math.fround(total));
}

function fftFrames(t, signal, p, dst) {
  const total = p[0];
  const frame = (t / total) | 0;
  const k = t % total;
  const window = Math.fround(0.5 - 0.5 * cosf(Math.fround(6.2831853 * Math.fround(k) / Math.fround((total - 1) | 0))));
  dst[Number(i64(2n * BigInt(t)))] = Math.fround(signal[(Math.imul(frame, p[1]) + k) | 0] * window);
  dst[Number(i64(i64(2n * BigInt(t)) + 1n))] = 0.0;
  return 0;
}

function spectrogramPixel(i, data, p) {
  const total = p[0];
  const frames = p[1];
  const rows = p[2];
  const x = i % frames;
  const bin = (((rows - 1) | 0) - ((i / frames) | 0)) | 0;
  const at = (Math.imul(Math.imul(x, total), 2) + Number(BigInt.asIntN(32, i64(2n * BigInt(bin))))) | 0;
  const re = data[at];
  const im = data[(at + 1) | 0];
  const magnitude = Math.fround(4.0 * sqrtf(Math.fround(Math.fround(re * re) + Math.fround(im * im))) / Math.fround(total));
  let level = Math.fround((20.0 * logf(Math.fround(magnitude + 0.0000001)) / 2.3025851 + 70.0) / 70.0);
  if (level < 0.0) {
    level = 0.0;
  }
  if (level > 1.0) {
    level = 1.0;
  }
  return level;
}

function complexMultiply(k, a, b, out) {
  out[Number(i64(2n * BigInt(k)))] = Math.fround(Math.fround(a[Number(i64(2n * BigInt(k)))] * b[Number(i64(2n * BigInt(k)))]) - Math.fround(a[Number(i64(i64(2n * BigInt(k)) + 1n))] * b[Number(i64(i64(2n * BigInt(k)) + 1n))]));
  out[Number(i64(i64(2n * BigInt(k)) + 1n))] = Math.fround(Math.fround(a[Number(i64(2n * BigInt(k)))] * b[Number(i64(i64(2n * BigInt(k)) + 1n))]) + Math.fround(a[Number(i64(i64(2n * BigInt(k)) + 1n))] * b[Number(i64(2n * BigInt(k)))]));
  return 0;
}

function realScaled(i, data, p) {
  return Math.fround(data[Number(i64(2n * BigInt(i)))] / p[0]);
}

export { fftStage, fftMagnitude, fftFrames, spectrogramPixel, complexMultiply, realScaled };
