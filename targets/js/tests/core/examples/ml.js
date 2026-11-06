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

function relu(x) {
  if (x < 0.0) {
    return 0.0;
  }
  return x;
}

function xorForward(x1, x2) {
  const w1 = new Grid([2, 2], 0);
  w1.set(0n, 0n, 1.0);
  w1.set(0n, 1n, 1.0);
  w1.set(1n, 0n, 1.0);
  w1.set(1n, 1n, 1.0);
  const b1 = new Grid([2], 0);
  b1[0] = 0.0;
  b1[1] = 0.0 - 1.0;
  const hidden = new Grid([2], 0);
  for (let i = 0n; i < 2n; i = i64(i + 1n)) {
    let preActivation = b1[Number(i)];
    preActivation = preActivation + w1.get(i, 0n) * x1;
    preActivation = preActivation + w1.get(i, 1n) * x2;
    hidden[Number(i)] = relu(preActivation);
  }
  const w2 = new Grid([2], 0);
  w2[0] = 1.0;
  w2[1] = 0.0 - 2.0;
  const b2 = 0.0;
  let output = b2;
  for (let i = 0n; i < 2n; i = i64(i + 1n)) {
    output = output + w2[Number(i)] * hidden[Number(i)];
  }
  return output;
}

function xorPredict(x1, x2) {
  if (xorForward(x1, x2) >= 0.5) {
    return 1n;
  }
  return 0n;
}

export { relu, xorForward, xorPredict };
