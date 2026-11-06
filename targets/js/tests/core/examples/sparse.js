"use strict";

// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero.
class Sparse {
  constructor(zero) {
    this.rows = [];
    this.cols = [];
    this.values = [];
    this.zero = zero;
  }

  get length() {
    return this.values.length;
  }

  get(row, col) {
    let found = this.zero;
    for (let i = 0; i < this.values.length; i++) {
      if (this.rows[i] === row && this.cols[i] === col) {
        found = this.values[i];
      }
    }
    return found;
  }

  set(row, col, value) {
    this.rows.push(row);
    this.cols.push(col);
    this.values.push(value);
  }

  copy() {
    const out = new Sparse(this.zero);
    out.rows = [...this.rows];
    out.cols = [...this.cols];
    out.values = [...this.values];
    return out;
  }
}

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function writeAndReadBack() {
  const sp = new Sparse(0n);
  sp.set(0n, 0n, 10n);
  sp.set(2n, 3n, 20n);
  sp.set(5n, 1n, 30n);
  return i64(i64(i64(sp.get(2n, 3n) * 100n) + i64(sp.get(5n, 1n) * 10n)) + sp.get(0n, 0n));
}

function unsetCellIsZero() {
  const sp = new Sparse(0n);
  sp.set(1n, 1n, 99n);
  return sp.get(9n, 9n);
}

function lastWriteWins() {
  const sp = new Sparse(0n);
  sp.set(3n, 3n, 1n);
  sp.set(3n, 3n, 2n);
  sp.set(3n, 3n, 3n);
  return sp.get(3n, 3n);
}

function oneDimensionalRoundTrip() {
  const sp = new Sparse(0n);
  sp.set(0n, 0n, 100n);
  sp.set(7n, 0n, 200n);
  sp.set(3n, 0n, 300n);
  return i64(i64(i64(sp.get(7n, 0n) * 100n) + i64(sp.get(3n, 0n) * 10n)) + sp.get(0n, 0n));
}

function lenAfterWrites() {
  const sp = new Sparse(0n);
  sp.set(0n, 0n, 1n);
  sp.set(1n, 1n, 2n);
  sp.set(1n, 1n, 3n);
  return BigInt(sp.length);
}

export { writeAndReadBack, unsetCellIsZero, lastWriteWins, oneDimensionalRoundTrip, lenAfterWrites };
