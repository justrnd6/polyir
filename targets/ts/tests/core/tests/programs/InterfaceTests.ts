import { Rect, Shape, Square, chosen, directCall, interfaceArgument, sumAll, twoInterfaces, viaField } from "./../../examples/interfaces.js";
import { off, offInt } from "./Check.js";

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

function checkConversionAtACall(): number {
  return offInt(directCall(), 920n);
}

function checkConversionAtAReturn(): number {
  return offInt(chosen(), 36034n);
}

function checkDarrayOfInterfaceValues(): number {
  return offInt(sumAll(), 41n);
}

function checkInterfaceAsAStructField(): number {
  return offInt(viaField(), 307n);
}

function checkOneStructTwoInterfaces(): number {
  return offInt(twoInterfaces(), 1016n);
}

function checkInterfaceMethodTakingAnInterface(): number {
  return offInt(interfaceArgument(), 109n);
}

function checkDispatchFollowsTheValue(): number {
  const a: Shape = new Square(5n);
  const b: Shape = new Rect(5n, 2n);
  return offInt(a.area(), 25n) + offInt(b.area(), 10n);
}

function checkSameMethodNameOnTwoStructs(): number {
  const s: Square = new Square(3n);
  const r: Rect = new Rect(3n, 4n);
  return offInt(i64(s.area() + r.area()), 21n);
}

function checkConvertedValueIsACopy(): number {
  const s: Square = new Square(4n);
  const a: Shape = s;
  const b: Shape = s;
  return offInt(i64(a.area() + b.area()), 32n);
}

interface Scaler {
  half(p0: number): number;
  offset(p0: number): bigint;
}

class Halver {
  constructor(public base: bigint) {}

  half(x: number): number {
    return Math.fround(x / 2.0);
  }

  offset(by: number): bigint {
    return i64(this.base + BigInt(by));
  }
}

function checkArgumentWidthsFollowTheInterface(): number {
  const s: Scaler = new Halver(40n);
  return off(s.half(9.0), 4.5) + offInt(s.offset(2), 42n);
}

function checkInterfaceInAnEither(): number {
  const r: Either<Shape, bigint> = Either.ok<Shape, bigint>(new Rect(4n, 4n));
  if (!r.isOk) {
    return 1.0;
  }
  const s: Shape = r.okValue;
  return offInt(s.area(), 16n);
}

function checkInterfaceCapturedByAClosure(): number {
  const s: Shape = new Square(7n);
  const f: () => bigint = () => s.area();
  return offInt(f(), 49n);
}

function checkInterfaceThroughAVar(): number {
  let s: Shape = new Square(2n);
  const first: bigint = s.area();
  s = new Rect(3n, 5n);
  return offInt(i64(i64(first * 100n) + s.area()), 415n);
}

interface Weighted {
  weight(): bigint;
}

function totalWeight(a: Weighted, b: Weighted): bigint {
  return i64(a.weight() + b.weight());
}

function checkGenericStructConvertsToAnInterface(): number {
  const a: Cell<bigint> = new Cell(5n);
  const b: Cell<number> = new Cell(2.5);
  return offInt(totalWeight(a, b), 14n) + offInt(a.get(), 5n) + off(Math.fround(b.get()), 2.5);
}

function checkGenericInterfaceValuesInADarray(): number {
  const all: Array<Weighted> = [];
  const a: Cell<bigint> = new Cell(1n);
  all.push(a);
  all.push(a);
  return offInt(i64(all[0].weight() + all[1].weight()), 14n);
}

class Cell<T> {
  constructor(public value: T) {}

  weight(): bigint {
    return 7n;
  }

  get(): T {
    return this.value;
  }
}

export { checkConversionAtACall, checkConversionAtAReturn, checkDarrayOfInterfaceValues, checkInterfaceAsAStructField, checkOneStructTwoInterfaces, checkInterfaceMethodTakingAnInterface, checkDispatchFollowsTheValue, checkSameMethodNameOnTwoStructs, checkConvertedValueIsACopy, Scaler, Halver, checkArgumentWidthsFollowTheInterface, checkInterfaceInAnEither, checkInterfaceCapturedByAClosure, checkInterfaceThroughAVar, Weighted, totalWeight, checkGenericStructConvertsToAnInterface, checkGenericInterfaceValuesInADarray, Cell };
