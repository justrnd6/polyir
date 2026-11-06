"use strict";

import { Rect, Shape, Square, chosen, directCall, interfaceArgument, sumAll, twoInterfaces, viaField } from "./../../examples/interfaces.js";
import { off, offInt } from "./Check.js";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

// An `either` holds its value as an `Ok` or an `Err`.
class Ok {
  constructor(value) {
    this.value = value;
  }
}

class Err {
  constructor(value) {
    this.value = value;
  }
}

function checkConversionAtACall() {
  return offInt(directCall(), 920n);
}

function checkConversionAtAReturn() {
  return offInt(chosen(), 36034n);
}

function checkDarrayOfInterfaceValues() {
  return offInt(sumAll(), 41n);
}

function checkInterfaceAsAStructField() {
  return offInt(viaField(), 307n);
}

function checkOneStructTwoInterfaces() {
  return offInt(twoInterfaces(), 1016n);
}

function checkInterfaceMethodTakingAnInterface() {
  return offInt(interfaceArgument(), 109n);
}

function checkDispatchFollowsTheValue() {
  const a = new Square(5n);
  const b = new Rect(5n, 2n);
  return offInt(a.area(), 25n) + offInt(b.area(), 10n);
}

function checkSameMethodNameOnTwoStructs() {
  const s = new Square(3n);
  const r = new Rect(3n, 4n);
  return offInt(i64(s.area() + r.area()), 21n);
}

function checkConvertedValueIsACopy() {
  const s = new Square(4n);
  const a = s;
  const b = s;
  return offInt(i64(a.area() + b.area()), 32n);
}

class Halver {
  constructor(base) {
    this.base = base;
  }

  half(x) {
    return Math.fround(x / 2.0);
  }

  offset(by) {
    return i64(this.base + BigInt(by));
  }
}

function checkArgumentWidthsFollowTheInterface() {
  const s = new Halver(40n);
  return off(s.half(9.0), 4.5) + offInt(s.offset(2), 42n);
}

function checkInterfaceInAnEither() {
  const r = new Ok(new Rect(4n, 4n));
  if (r instanceof Err) {
    return 1.0;
  }
  const s = r.value;
  return offInt(s.area(), 16n);
}

function checkInterfaceCapturedByAClosure() {
  const s = new Square(7n);
  const f = () => s.area();
  return offInt(f(), 49n);
}

function checkInterfaceThroughAVar() {
  let s = new Square(2n);
  const first = s.area();
  s = new Rect(3n, 5n);
  return offInt(i64(i64(first * 100n) + s.area()), 415n);
}

function totalWeight(a, b) {
  return i64(a.weight() + b.weight());
}

function checkGenericStructConvertsToAnInterface() {
  const a = new Cell(5n);
  const b = new Cell(2.5);
  return offInt(totalWeight(a, b), 14n) + offInt(a.get(), 5n) + off(Math.fround(b.get()), 2.5);
}

function checkGenericInterfaceValuesInADarray() {
  const all = [];
  const a = new Cell(1n);
  all.push(a);
  all.push(a);
  return offInt(i64(all[0].weight() + all[1].weight()), 14n);
}

class Cell {
  constructor(value) {
    this.value = value;
  }

  weight() {
    return 7n;
  }

  get() {
    return this.value;
  }
}

export { checkConversionAtACall, checkConversionAtAReturn, checkDarrayOfInterfaceValues, checkInterfaceAsAStructField, checkOneStructTwoInterfaces, checkInterfaceMethodTakingAnInterface, checkDispatchFollowsTheValue, checkSameMethodNameOnTwoStructs, checkConvertedValueIsACopy, Scaler, Halver, checkArgumentWidthsFollowTheInterface, checkInterfaceInAnEither, checkInterfaceCapturedByAClosure, checkInterfaceThroughAVar, Weighted, totalWeight, checkGenericStructConvertsToAnInterface, checkGenericInterfaceValuesInADarray, Cell };
