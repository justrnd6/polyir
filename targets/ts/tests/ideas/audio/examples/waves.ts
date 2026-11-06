import { fade, noteFrequency, sine } from "./../Wave.js";

function concertAHeight(): bigint {
  const quarterCycle: number = Math.fround(0.25 / noteFrequency(69.0));
  return BigInt(Math.trunc(Math.fround(sine(Math.fround(noteFrequency(69.0) * quarterCycle)) * 1000.0)));
}

function softAttack(): bigint {
  return BigInt(Math.trunc(Math.fround(fade(0.5, 1.0, 1.0, 4.0) * 1000.0)));
}

function chordSample(t: number): number {
  return Math.fround(0.3 * Math.fround(Math.fround(sine(Math.fround(noteFrequency(60.0) * t)) + sine(Math.fround(noteFrequency(64.0) * t))) + sine(Math.fround(noteFrequency(67.0) * t))));
}

export { concertAHeight, softAttack, chordSample };
