import { off, offInt } from "./Check.js";

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function checkFloatRemainder(): number {
  const a: number = 7.5;
  const b: number = 2.0;
  return off(a % b, 1.5);
}

function checkFloatRemainderKeepsTheSignOfTheLeftNumber(): number {
  const a: number = Math.fround(0.0 - 7.5);
  const b: number = 2.0;
  return off(a % b, Math.fround(0.0 - 1.5));
}

function checkFloatRemainderIgnoresTheSignOfTheRightNumber(): number {
  const a: number = 7.5;
  const b: number = Math.fround(0.0 - 2.0);
  return off(a % b, 1.5);
}

function checkFloatRemainderOfNumbersThatDivideEvenly(): number {
  const b: number = 47.0;
  const c: number = 97.0;
  return off(Math.fround(2867.0 % b), 0.0) + off(Math.fround(5969.0 % b), 0.0) + off(Math.fround(9409.0 % c), 0.0);
}

function checkFloatRemainderOfZero(): number {
  const a: number = 0.0;
  const b: number = 5.0;
  return off(a % b, 0.0);
}

function checkFloatRemainderOfAFractionOfOne(): number {
  const a: number = 0.75;
  const b: number = 1.0;
  return off(a % b, 0.75);
}

function checkWholeNumberRemainder(): number {
  const a: bigint = 17n;
  const b: bigint = 5n;
  return offInt(a % b, 2n) + offInt(i64(0n - a) % b, i64(0n - 2n));
}

export { checkFloatRemainder, checkFloatRemainderKeepsTheSignOfTheLeftNumber, checkFloatRemainderIgnoresTheSignOfTheRightNumber, checkFloatRemainderOfNumbersThatDivideEvenly, checkFloatRemainderOfZero, checkFloatRemainderOfAFractionOfOne, checkWholeNumberRemainder };
