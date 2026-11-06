// A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(i, j, value)`
// writes one.
class Grid<T> {
  readonly cells: T[];

  constructor(readonly dims: number[], zero: T) {
    this.cells = Array.from({ length: dims.reduce((count, size) => count * size, 1) }, () => zero);
  }

  place(indices: bigint[]): number {
    let at = 0;
    for (let i = 0; i < this.dims.length; i++) {
      at = at * this.dims[i] + Number(indices[i]);
    }
    return at;
  }

  get(...indices: bigint[]): T {
    return this.cells[this.place(indices)];
  }

  set(...indicesAndValue: any[]): void {
    const value = indicesAndValue.pop();
    this.cells[this.place(indicesAndValue)] = value;
  }

  copy(): Grid<T> {
    const out = new Grid<T>(this.dims, this.cells[0]);
    out.cells.splice(0, out.cells.length, ...this.cells);
    return out;
  }
}

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function relu(x: number): number {
  if (x < 0.0) {
    return 0.0;
  }
  return x;
}

function xorForward(x1: number, x2: number): number {
  const w1: Grid<number> = new Grid([2, 2], 0);
  w1.set(0n, 0n, 1.0);
  w1.set(0n, 1n, 1.0);
  w1.set(1n, 0n, 1.0);
  w1.set(1n, 1n, 1.0);
  const b1: Grid<number> = new Grid([2], 0);
  b1[0] = 0.0;
  b1[1] = 0.0 - 1.0;
  const hidden: Grid<number> = new Grid([2], 0);
  for (let i = 0n; i < 2n; i = i64(i + 1n)) {
    let preActivation: number = b1[Number(i)];
    preActivation = preActivation + w1.get(i, 0n) * x1;
    preActivation = preActivation + w1.get(i, 1n) * x2;
    hidden[Number(i)] = relu(preActivation);
  }
  const w2: Grid<number> = new Grid([2], 0);
  w2[0] = 1.0;
  w2[1] = 0.0 - 2.0;
  const b2: number = 0.0;
  let output: number = b2;
  for (let i = 0n; i < 2n; i = i64(i + 1n)) {
    output = output + w2[Number(i)] * hidden[Number(i)];
  }
  return output;
}

function xorPredict(x1: number, x2: number): bigint {
  if (xorForward(x1, x2) >= 0.5) {
    return 1n;
  }
  return 0n;
}

export { relu, xorForward, xorPredict };
