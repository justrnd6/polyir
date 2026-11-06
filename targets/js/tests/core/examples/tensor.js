"use strict";

// A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(i, j, value)`
// writes one.
class Grid {
  constructor(dims, zero) {
    this.dims = dims;
    this.cells = Array.from({ length: dims.reduce((count, size) => count * size, 1) }, () => zero);
  }

  place(indices) {
    let at = 0;
    for (let i = 0; i < this.dims.length; i++) {
      at = at * this.dims[i] + Number(indices[i]);
    }
    return at;
  }

  get(...indices) {
    return this.cells[this.place(indices)];
  }

  set(...indicesAndValue) {
    const value = indicesAndValue.pop();
    this.cells[this.place(indicesAndValue)] = value;
  }

  copy() {
    const out = new Grid(this.dims, null);
    out.cells = [...this.cells];
    return out;
  }
}

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function weightedSum3D() {
  const t = new Grid([2, 3, 4], 0n);
  for (let i = 0n; i < 2n; i = i64(i + 1n)) {
    for (let j = 0n; j < 3n; j = i64(j + 1n)) {
      for (let k = 0n; k < 4n; k = i64(k + 1n)) {
        t.set(i, j, k, i64(i64(i64(i * 100n) + i64(j * 10n)) + k));
      }
    }
  }
  let total = 0n;
  for (let i = 0n; i < 2n; i = i64(i + 1n)) {
    for (let j = 0n; j < 3n; j = i64(j + 1n)) {
      for (let k = 0n; k < 4n; k = i64(k + 1n)) {
        total = i64(total + i64(i64(t.get(i, j, k) * i64(i + 1n)) * i64(j + 1n)));
      }
    }
  }
  return total;
}

function cornerRoundTrip() {
  const t = new Grid([2, 2, 2], 0n);
  t.set(0n, 0n, 0n, 1n);
  t.set(0n, 0n, 1n, 2n);
  t.set(0n, 1n, 0n, 3n);
  t.set(1n, 0n, 0n, 4n);
  t.set(1n, 1n, 1n, 5n);
  return i64(i64(i64(t.get(1n, 1n, 1n) * 100n) + i64(t.get(1n, 0n, 0n) * 10n)) + t.get(0n, 1n, 0n));
}

function dimsCheck() {
  const t = new Grid([2, 3, 4], 0n);
  return i64(i64(i64(BigInt(t.dims[0]) * 100n) + i64(BigInt(t.dims[1]) * 10n)) + BigInt(t.dims[2]));
}

function oneDimensional() {
  const t = new Grid([4], 0n);
  t[0] = 10n;
  t[1] = 20n;
  t[2] = 30n;
  t[3] = 40n;
  return i64(t[1] + t[2]);
}

function fourDimensional() {
  const t = new Grid([2, 2, 2, 2], 0n);
  t.set(0n, 0n, 0n, 0n, 1n);
  t.set(1n, 1n, 1n, 1n, 99n);
  t.set(0n, 1n, 0n, 1n, 5n);
  return i64(i64(t.get(1n, 1n, 1n, 1n) * 100n) + t.get(0n, 1n, 0n, 1n));
}

export { weightedSum3D, cornerRoundTrip, dimsCheck, oneDimensional, fourDimensional };
