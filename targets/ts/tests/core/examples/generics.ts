// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

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

function boxedI64(): bigint {
  const b: Box<bigint> = new Box(42n);
  return b.getValue();
}

function boxedBoth(): bigint {
  const bi: Box<bigint> = new Box(100n);
  const bd: Box<number> = new Box(2.0);
  return i64(bi.getValue() + BigInt(Math.trunc(bd.getValue())));
}

function pairOfIntAndDouble(): bigint {
  const p: Pair_i64_double = new Pair_i64_double(7n, 8.5);
  return p.sumAsInt();
}

function doubledI64(): bigint {
  const d: Doubler_i64 = new Doubler_i64(21n);
  return d.doubled();
}

function containerOk(): bigint {
  const c: Container<bigint> = new Container(Either.ok<bigint, bigint>(42n));
  return c.unwrapOrDefault(0n);
}

function makePairResult(ok: bigint): Either<Pair_i64_i64, bigint> {
  if (ok === 0n) {
    return Either.err<Pair_i64_i64, bigint>(-1n);
  }
  return Either.ok<Pair_i64_i64, bigint>(new Pair_i64_i64(11n, 22n));
}

function pairResultOrErr(ok: bigint): bigint {
  const r: Either<Pair_i64_i64, bigint> = makePairResult(ok);
  if (!r.isOk) {
    return r.errValue;
  }
  const p: Pair_i64_i64 = r.okValue;
  return i64(i64(p.a * 100n) + p.b);
}

class Box<T> {
  constructor(public value: T) {}

  getValue(): T {
    return this.value;
  }
}

class Pair_i64_double {
  constructor(public a: bigint, public b: number) {}

  sumAsInt(): bigint {
    return i64(this.a + BigInt(Math.trunc(this.b)));
  }
}

class Doubler_i64 {
  constructor(public value: bigint) {}

  doubled(): bigint {
    return i64(this.value + this.value);
  }
}

class Container<T> {
  constructor(public value: Either<T, bigint>) {}

  unwrapOrDefault(def: T): T {
    if (!this.value.isOk) {
      return def;
    }
    return this.value.okValue;
  }
}

class Pair_i64_i64 {
  constructor(public a: bigint, public b: bigint) {}

  sumAsInt(): bigint {
    return i64(this.a + this.b);
  }
}

export { boxedI64, boxedBoth, pairOfIntAndDouble, doubledI64, containerOk, makePairResult, pairResultOrErr, Box, Pair_i64_double, Doubler_i64, Container, Pair_i64_i64 };
