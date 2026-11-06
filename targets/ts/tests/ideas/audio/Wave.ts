import { clamp, fract } from "./../../core/stdlib/Math.js";

const sinf = (x: number): number => Math.fround(Math.sin(x));
const fabsf = (x: number): number => Math.fround(Math.abs(x));
const powf = (x: number, y: number): number => Math.fround(Math.pow(x, y));

function sine(phase: number): number {
  return sinf(Math.fround(6.2831853 * fract(phase)));
}

function square(phase: number): number {
  if (fract(phase) < 0.5) {
    return 1.0;
  }
  return Math.fround(0.0 - 1.0);
}

function sawtooth(phase: number): number {
  return Math.fround(2.0 * fract(phase) - 1.0);
}

function triangle(phase: number): number {
  return Math.fround(1.0 - 4.0 * fabsf(Math.fround(fract(phase) - 0.5)));
}

function noise(seed: number): number {
  const a: number = ((Math.imul(seed, 75) + 74) | 0) % 65537;
  const b: number = ((Math.imul(a, 75) + 74) | 0) % 65537;
  return Math.fround(2.0 * Math.fround(b) / 65537.0 - 1.0);
}

function fade(t: number, attack: number, release: number, total: number): number {
  let level: number = t < attack ? Math.fround(t / attack) : 1.0;
  if (Math.fround(total - t) < release) {
    level = Math.fround(level * clamp(Math.fround(Math.fround(total - t) / release), 0.0, 1.0));
  }
  return level;
}

function noteFrequency(m: number): number {
  return Math.fround(440.0 * powf(2.0, Math.fround(Math.fround(m - 69.0) / 12.0)));
}

export { sine, square, sawtooth, triangle, noise, fade, noteFrequency };
