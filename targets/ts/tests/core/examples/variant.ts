// An `either`: an ok value or an error, and which one it holds.
class Either<T, E> {
  private constructor(readonly isOk: boolean, private readonly held: T | E) {}

  static ok<T, E>(value: T): Either<T, E> {
    return new Either<T, E>(true, value);
  }

  static err<T, E>(value: E): Either<T, E> {
    return new Either<T, E>(false, value);
  }

  get okValue(): T {
    return this.held as T;
  }

  get errValue(): E {
    return this.held as E;
  }
}

// A variant: the number of its case and the value it holds.
class Variant {
  constructor(readonly tag: bigint, readonly value: any) {}
}

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function safeDivide(a: bigint, b: bigint): Either<bigint, bigint> {
  if (b === 0n) {
    return Either.err<bigint, bigint>(99n);
  }
  return Either.ok<bigint, bigint>(a / b);
}

function divideOrDefault(a: bigint, b: bigint): bigint {
  const r: Either<bigint, bigint> = safeDivide(a, b);
  if (!r.isOk) {
    return r.errValue;
  }
  return r.okValue;
}

function eitherCoercion(): bigint {
  const r: Either<number, bigint> = Either.ok<number, bigint>(3.0);
  return BigInt(Math.trunc(r.okValue));
}

function variantSum(which: bigint): bigint {
  let v: Variant = new Variant(0n, 0n);
  if (which === 0n) {
    v = new Variant(0n, 10n);
  }
  if (which === 1n) {
    v = new Variant(1n, 2.5);
  }
  if (which === 2n) {
    v = new Variant(2n, 30n);
  }
  if (v.tag === 0n) {
    return v.value;
  }
  if (v.tag === 1n) {
    return i64(BigInt(Math.trunc(v.value)) * 100n);
  }
  return i64(v.value * 1000n);
}

function castRoundTrip(): bigint {
  const d: number = 9.0;
  const asI64: bigint = BigInt(Math.trunc(d));
  const backToD: number = Number(asI64);
  return i64(BigInt(Math.trunc(backToD)) + asI64);
}

export { safeDivide, divideOrDefault, eitherCoercion, variantSum, castRoundTrip };
