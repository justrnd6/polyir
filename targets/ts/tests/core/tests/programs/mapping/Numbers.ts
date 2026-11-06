declare const process: any;
export {};

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

// An unsigned 64-bit integer is a bigint, brought back into 0 .. 2^64 - 1 after an operation that can leave them.
function u64(value: bigint): bigint {
  return BigInt.asUintN(64, value);
}

function mix(h: bigint, v: bigint): bigint {
  return i64(i64(h * 31n) + v) % 1000003n;
}

function wide(): bigint {
  const big: bigint = 9223372036854775807n;
  const wrapped: bigint = i64(big + 1n);
  const product: bigint = i64(3037000500n * 3037000500n);
  const neg: bigint = i64(0n - big);
  const quotient: bigint = i64(0n - 7n) / 2n;
  const rest: bigint = i64(0n - 7n) % 3n;
  let h: bigint = 7n;
  h = mix(h, (wrapped >> 40n) % 1000n);
  h = mix(h, (product >> 33n) % 1000n);
  h = mix(h, (neg >> 50n) % 1000n);
  h = mix(h, i64(quotient + 100n));
  h = mix(h, i64(rest + 100n));
  h = mix(h, i64(big << 3n) >> 60n);
  h = mix(h, i64(0n - 1n) >> i64(5n + 100n));
  return h;
}

function thirtyTwo(): bigint {
  const big: number = 2147483647;
  const wrapped: number = (big + 1) | 0;
  const product: number = Number(BigInt.asIntN(32, i64(65536n * 65536n)));
  const other: number = Number(BigInt.asIntN(32, i64(123456789n * 987n)));
  const quotient: number = Number(BigInt.asIntN(32, i64(0n - 7n) / 2n));
  const rest: number = Number(BigInt.asIntN(32, i64(0n - 7n) % 3n));
  let h: bigint = 11n;
  h = mix(h, BigInt(wrapped) % 100003n);
  h = mix(h, i64(BigInt(product) + 5n));
  h = mix(h, BigInt(other) % 100003n);
  h = mix(h, i64(BigInt(quotient) + 100n));
  h = mix(h, i64(BigInt(rest) + 100n));
  h = mix(h, BigInt(wrapped >> 7) % 100003n);
  h = mix(h, BigInt(big << 5) % 100003n);
  return h;
}

function unsigned32(): bigint {
  const top: number = 4294967295;
  const wrapped: number = (top + 2) >>> 0;
  const product: number = Number(BigInt.asUintN(32, i64(4000000000n * 7n)));
  const under: number = Number(BigInt.asUintN(32, i64(3n - 5n)));
  const shifted: number = (top << 4) >>> 0;
  let h: bigint = 13n;
  h = mix(h, BigInt(wrapped));
  h = mix(h, BigInt(product) % 100003n);
  h = mix(h, BigInt(under) % 100003n);
  h = mix(h, BigInt(shifted) % 100003n);
  h = mix(h, BigInt(top >>> 3) % 100003n);
  h = mix(h, BigInt((top / 3) >>> 0) % 100003n);
  h = mix(h, BigInt(top % 1000));
  return h;
}

function narrow(): bigint {
  const a: number = 127;
  const b: number = (a + 1) << 24 >> 24;
  const c: number = 32767;
  const d: number = (c + 1) << 16 >> 16;
  const e: number = 250;
  const f: number = (e + 10) & 255;
  const g: number = 65535;
  const hh: number = (g + 3) & 65535;
  const m: number = Number(BigInt.asIntN(8, i64(0n - 128n)));
  const n: number = (m / Number(BigInt.asIntN(8, i64(0n - 1n)))) << 24 >> 24;
  let h: bigint = 17n;
  h = mix(h, i64(BigInt(b) + 200n));
  h = mix(h, i64(BigInt(d) + 40000n));
  h = mix(h, BigInt(f));
  h = mix(h, BigInt(hh));
  h = mix(h, i64(BigInt(n) + 200n));
  h = mix(h, i64(BigInt(b >> 2) + 200n));
  h = mix(h, BigInt((e << 3) & 255));
  return h;
}

function casts(): bigint {
  const big: bigint = 5000000000n;
  const cut: number = Number(BigInt.asIntN(32, big));
  const tiny: number = Number(BigInt.asIntN(8, 300n));
  const uns: number = Number(BigInt.asUintN(32, i64(0n - 5n)));
  const up: bigint = u64(i64(0n - 1n));
  const back: bigint = i64(up);
  const f: number = 3.99;
  const g: number = 0.0 - 3.99;
  let h: bigint = 19n;
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

function main(): bigint {
  let h: bigint = 1n;
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
