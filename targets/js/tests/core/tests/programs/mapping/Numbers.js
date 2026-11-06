"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

// An unsigned 64-bit integer is a BigInt, brought back into 0 .. 2^64 - 1 after an operation that can leave them.
function u64(value) {
  return BigInt.asUintN(64, value);
}

function mix(h, v) {
  return i64(i64(h * 31n) + v) % 1000003n;
}

function wide() {
  const big = 9223372036854775807n;
  const wrapped = i64(big + 1n);
  const product = i64(3037000500n * 3037000500n);
  const neg = i64(0n - big);
  const quotient = i64(0n - 7n) / 2n;
  const rest = i64(0n - 7n) % 3n;
  let h = 7n;
  h = mix(h, (wrapped >> 40n) % 1000n);
  h = mix(h, (product >> 33n) % 1000n);
  h = mix(h, (neg >> 50n) % 1000n);
  h = mix(h, i64(quotient + 100n));
  h = mix(h, i64(rest + 100n));
  h = mix(h, i64(big << 3n) >> 60n);
  h = mix(h, i64(0n - 1n) >> i64(5n + 100n));
  return h;
}

function thirtyTwo() {
  const big = 2147483647;
  const wrapped = (big + 1) | 0;
  const product = Number(BigInt.asIntN(32, i64(65536n * 65536n)));
  const other = Number(BigInt.asIntN(32, i64(123456789n * 987n)));
  const quotient = Number(BigInt.asIntN(32, i64(0n - 7n) / 2n));
  const rest = Number(BigInt.asIntN(32, i64(0n - 7n) % 3n));
  let h = 11n;
  h = mix(h, BigInt(wrapped) % 100003n);
  h = mix(h, i64(BigInt(product) + 5n));
  h = mix(h, BigInt(other) % 100003n);
  h = mix(h, i64(BigInt(quotient) + 100n));
  h = mix(h, i64(BigInt(rest) + 100n));
  h = mix(h, BigInt(wrapped >> 7) % 100003n);
  h = mix(h, BigInt(big << 5) % 100003n);
  return h;
}

function unsigned32() {
  const top = 4294967295;
  const wrapped = (top + 2) >>> 0;
  const product = Number(BigInt.asUintN(32, i64(4000000000n * 7n)));
  const under = Number(BigInt.asUintN(32, i64(3n - 5n)));
  const shifted = (top << 4) >>> 0;
  let h = 13n;
  h = mix(h, BigInt(wrapped));
  h = mix(h, BigInt(product) % 100003n);
  h = mix(h, BigInt(under) % 100003n);
  h = mix(h, BigInt(shifted) % 100003n);
  h = mix(h, BigInt(top >>> 3) % 100003n);
  h = mix(h, BigInt((top / 3) >>> 0) % 100003n);
  h = mix(h, BigInt(top % 1000));
  return h;
}

function narrow() {
  const a = 127;
  const b = (a + 1) << 24 >> 24;
  const c = 32767;
  const d = (c + 1) << 16 >> 16;
  const e = 250;
  const f = (e + 10) & 255;
  const g = 65535;
  const hh = (g + 3) & 65535;
  const m = Number(BigInt.asIntN(8, i64(0n - 128n)));
  const n = (m / Number(BigInt.asIntN(8, i64(0n - 1n)))) << 24 >> 24;
  let h = 17n;
  h = mix(h, i64(BigInt(b) + 200n));
  h = mix(h, i64(BigInt(d) + 40000n));
  h = mix(h, BigInt(f));
  h = mix(h, BigInt(hh));
  h = mix(h, i64(BigInt(n) + 200n));
  h = mix(h, i64(BigInt(b >> 2) + 200n));
  h = mix(h, BigInt((e << 3) & 255));
  return h;
}

function casts() {
  const big = 5000000000n;
  const cut = Number(BigInt.asIntN(32, big));
  const tiny = Number(BigInt.asIntN(8, 300n));
  const uns = Number(BigInt.asUintN(32, i64(0n - 5n)));
  const up = u64(i64(0n - 1n));
  const back = i64(up);
  const f = 3.99;
  const g = 0.0 - 3.99;
  let h = 19n;
  h = mix(h, BigInt(cut) % 100003n);
  h = mix(h, i64(BigInt(tiny) + 200n));
  h = mix(h, BigInt(uns) % 100003n);
  h = mix(h, i64(up >> 60n));
  h = mix(h, i64(back + 5n));
  h = mix(h, BigInt(Math.trunc(f)));
  h = mix(h, i64(BigInt(Math.trunc(g)) + 10n));
  h = mix(h, BigInt(Math.trunc(f * 2.5)));
  return h;
}

function main() {
  let h = 1n;
  h = mix(h, wide());
  h = mix(h, thirtyTwo());
  h = mix(h, unsigned32());
  h = mix(h, narrow());
  h = mix(h, casts());
  return h % 251n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
