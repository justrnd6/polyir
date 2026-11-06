import { clamp, fract, mix, sign } from "./../../../core/stdlib/Math.js";
import { off, offInt } from "./../../../core/tests/programs/Check.js";
import { fade, noise, noteFrequency, sawtooth, sine, square, triangle } from "./../Wave.js";

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function checkSignOfNegative(): number {
  return off(sign(Math.fround(0.0 - 3.0)), Math.fround(0.0 - 1.0));
}

function checkSignOfZero(): number {
  return off(sign(0.0), 0.0);
}

function checkSignOfPositive(): number {
  return off(sign(2.0), 1.0);
}

function checkFractOfPositive(): number {
  return off(fract(2.75), 0.75);
}

function checkFractOfNegative(): number {
  return off(fract(Math.fround(0.0 - 0.25)), 0.75);
}

function checkClampAbove(): number {
  return off(clamp(5.0, Math.fround(0.0 - 1.0), 2.0), 2.0);
}

function checkClampBelow(): number {
  return off(clamp(Math.fround(0.0 - 5.0), Math.fround(0.0 - 1.0), 2.0), Math.fround(0.0 - 1.0));
}

function checkMix(): number {
  return off(mix(10.0, 20.0, 0.25), 12.5);
}

function checkSinePeak(): number {
  return off(sine(0.25), 1.0);
}

function checkSineTrough(): number {
  return off(sine(0.75), Math.fround(0.0 - 1.0));
}

function checkSineZeroCrossing(): number {
  return off(sine(1.5), 0.0);
}

function checkSquareHigh(): number {
  return off(square(0.25), 1.0);
}

function checkSquareLow(): number {
  return off(square(0.75), Math.fround(0.0 - 1.0));
}

function checkSawtoothStart(): number {
  return off(sawtooth(0.0), Math.fround(0.0 - 1.0));
}

function checkSawtoothThreeQuarters(): number {
  return off(sawtooth(0.75), 0.5);
}

function checkTriangleStart(): number {
  return off(triangle(0.0), Math.fround(0.0 - 1.0));
}

function checkTrianglePeak(): number {
  return off(triangle(0.5), 1.0);
}

function checkTriangleMiddle(): number {
  return off(triangle(0.25), 0.0);
}

function checkConcertA(): number {
  return off(noteFrequency(69.0), 440.0);
}

function checkOctaveAboveConcertA(): number {
  return off(noteFrequency(81.0), 880.0);
}

function checkFadeInHalfway(): number {
  return off(fade(0.5, 1.0, 1.0, 4.0), 0.5);
}

function checkFadeHolds(): number {
  return off(fade(2.0, 1.0, 1.0, 4.0), 1.0);
}

function checkFadeOutHalfway(): number {
  return off(fade(3.5, 1.0, 1.0, 4.0), 0.5);
}

function checkNoiseRange(): number {
  let outside: bigint = 0n;
  for (let seed = 0n; seed < 1000n; seed = i64(seed + 1n)) {
    if (noise(Number(BigInt.asIntN(32, seed))) > 1.0 || noise(Number(BigInt.asIntN(32, seed))) < Math.fround(0.0 - 1.0)) {
      outside = i64(outside + 1n);
    }
  }
  return offInt(outside, 0n);
}

function checkNoiseRepeats(): number {
  return off(Math.fround(noise(42) - noise(42)), 0.0);
}

function checkNoiseVaries(): number {
  if (noise(1) === noise(2) && noise(2) === noise(3)) {
    return 1.0;
  }
  return 0.0;
}

export { checkSignOfNegative, checkSignOfZero, checkSignOfPositive, checkFractOfPositive, checkFractOfNegative, checkClampAbove, checkClampBelow, checkMix, checkSinePeak, checkSineTrough, checkSineZeroCrossing, checkSquareHigh, checkSquareLow, checkSawtoothStart, checkSawtoothThreeQuarters, checkTriangleStart, checkTrianglePeak, checkTriangleMiddle, checkConcertA, checkOctaveAboveConcertA, checkFadeInHalfway, checkFadeHolds, checkFadeOutHalfway, checkNoiseRange, checkNoiseRepeats, checkNoiseVaries };
