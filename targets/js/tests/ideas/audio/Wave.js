"use strict";

import { clamp, fract } from "./../../core/stdlib/Math.js";

const sinf = (x) => Math.fround(Math.sin(x));
const fabsf = (x) => Math.fround(Math.abs(x));
const powf = (x, y) => Math.fround(Math.pow(x, y));

function sine(phase) {
  return sinf(Math.fround(6.2831853 * fract(phase)));
}

function square(phase) {
  if (fract(phase) < 0.5) {
    return 1.0;
  }
  return Math.fround(0.0 - 1.0);
}

function sawtooth(phase) {
  return Math.fround(2.0 * fract(phase) - 1.0);
}

function triangle(phase) {
  return Math.fround(1.0 - 4.0 * fabsf(Math.fround(fract(phase) - 0.5)));
}

function noise(seed) {
  const a = ((Math.imul(seed, 75) + 74) | 0) % 65537;
  const b = ((Math.imul(a, 75) + 74) | 0) % 65537;
  return Math.fround(2.0 * Math.fround(b) / 65537.0 - 1.0);
}

function fade(t, attack, release, total) {
  let level = t < attack ? Math.fround(t / attack) : 1.0;
  if (Math.fround(total - t) < release) {
    level = Math.fround(level * clamp(Math.fround(Math.fround(total - t) / release), 0.0, 1.0));
  }
  return level;
}

function noteFrequency(m) {
  return Math.fround(440.0 * powf(2.0, Math.fround(Math.fround(m - 69.0) / 12.0)));
}

export { sine, square, sawtooth, triangle, noise, fade, noteFrequency };
