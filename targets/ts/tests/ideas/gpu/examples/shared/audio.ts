import { sign } from "./../../../../core/stdlib/Math.js";
import { fade, sine } from "./../../../audio/Wave.js";

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

const floorf = (x: number): number => Math.fround(Math.floor(x));
const sinf = (x: number): number => Math.fround(Math.sin(x));

function synth(i: number, p: Array<number>): number {
  const t: number = Math.fround(Math.fround(i) / p[0]);
  const envelope: number = fade(t, p[2], p[3], p[4]);
  const wobble: number = Math.fround(p[6] * sine(Math.fround(p[5] * t)));
  let sum: number = 0.0;
  for (let k = 0n; k < BigInt(p[1] | 0); k = i64(k + 1n)) {
    const cycles: number = Math.fround(p[Number(i64(8n + i64(2n * k)))] * t);
    sum = Math.fround(sum + Math.fround(p[Number(i64(9n + i64(2n * k)))] * sinf(Math.fround(6.2831853 * Math.fround(cycles - floorf(cycles)) + wobble))));
  }
  return Math.fround(Math.fround(p[7] * envelope) * sum);
}

function squareWave(i: number, p: Array<number>): number {
  return sign(sine(Math.fround(Math.fround(p[1] * Math.fround(i)) / p[0])));
}

function partialSum(i: number, p: Array<number>): number {
  const x: number = Math.fround(6.2831853 * Math.fround(i % (p[0] | 0)) / p[0]);
  let sum: number = 0.0;
  for (let h = 1n; h <= BigInt(p[2] | 0); h = i64(h + 1n)) {
    const number: number = Math.fround(Number(h));
    const wave: number = sinf(Math.fround(number * x));
    if (p[1] < 0.5) {
      if (h % 2n === 1n) {
        sum = Math.fround(sum + Math.fround(wave / number));
      }
    }
    if (p[1] > 0.5 && p[1] < 1.5) {
      if (h % 2n === 1n) {
        sum = Math.fround(sum + Math.fround(wave / number));
      }
      if (h % 2n === 0n) {
        sum = Math.fround(sum - Math.fround(wave / number));
      }
    }
    if (p[1] > 1.5) {
      if (h % 4n === 1n) {
        sum = Math.fround(sum + Math.fround(wave / Math.fround(number * number)));
      }
      if (h % 4n === 3n) {
        sum = Math.fround(sum - Math.fround(wave / Math.fround(number * number)));
      }
    }
  }
  if (p[1] < 0.5) {
    return Math.fround(1.2732395 * sum);
  }
  if (p[1] < 1.5) {
    return Math.fround(0.6366198 * sum);
  }
  return Math.fround(0.8105695 * sum);
}

function melody(i: number, notes: Array<number>, p: Array<number>): number {
  const t: number = Math.fround(Math.fround(i) / p[0]);
  let sum: number = 0.0;
  for (let n = 0n; n < BigInt(p[1] | 0); n = i64(n + 1n)) {
    const start: number = notes[Number(i64(3n * n))];
    const length: number = notes[Number(i64(i64(3n * n) + 1n))];
    const local: number = Math.fround(t - start);
    if (local >= 0.0 && local < length) {
      const envelope: number = fade(local, 0.02, 0.08, length);
      const f: number = notes[Number(i64(i64(3n * n) + 2n))];
      sum = Math.fround(sum + Math.fround(envelope * Math.fround(Math.fround(sine(Math.fround(f * t)) + Math.fround(0.5 * sine(Math.fround(2.0 * f * t)))) + Math.fround(0.25 * sine(Math.fround(3.0 * f * t))))));
    }
  }
  return Math.fround(p[2] * sum);
}

export { synth, squareWave, partialSum, melody };
