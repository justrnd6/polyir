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

function fillAndWeight(scale) {
  const g = new Grid([3, 4], 0n);
  for (let r = 0n; r < 3n; r = i64(r + 1n)) {
    for (let c = 0n; c < 4n; c = i64(c + 1n)) {
      g.set(r, c, i64(i64(i64(r * 4n) + c) * scale));
    }
  }
  let total = 0n;
  for (let r = 0n; r < 3n; r = i64(r + 1n)) {
    for (let c = 0n; c < 4n; c = i64(c + 1n)) {
      total = i64(total + i64(g.get(r, c) * i64(r + 1n)));
    }
  }
  return total;
}

function singleCellRoundTrip() {
  const g = new Grid([2, 2], 0n);
  g.set(0n, 0n, 10n);
  g.set(0n, 1n, 20n);
  g.set(1n, 0n, 30n);
  g.set(1n, 1n, 40n);
  return i64(g.get(1n, 0n) - g.get(0n, 1n));
}

function dimsCheck() {
  const g = new Grid([3, 4], 0n);
  return i64(i64(BigInt(g.dims[0]) * 100n) + BigInt(g.dims[1]));
}

export { fillAndWeight, singleCellRoundTrip, dimsCheck };
