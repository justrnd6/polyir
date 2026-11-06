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

function literalEqualsItself(): bigint {
  const c: number = 65;
  if (c === 65) {
    return 1n;
  }
  return 0n;
}

function literalsDistinguishCorrectly(): bigint {
  const a: number = 97;
  const b: number = 98;
  if (a === b) {
    return 0n;
  }
  return 1n;
}

function gridOfCharsRoundTrip(): bigint {
  const g: Grid<number> = new Grid([1, 3], 0);
  g.set(0n, 0n, 120);
  g.set(0n, 1n, 121);
  g.set(0n, 2n, 122);
  if (g.get(0n, 0n) === 120 && g.get(0n, 1n) === 121 && g.get(0n, 2n) === 122) {
    return 1n;
  }
  return 0n;
}

function darrayOfCharsRoundTrip(): bigint {
  const cs: Array<number> = [];
  cs.push(104);
  cs.push(105);
  if (cs[0] === 104 && cs[1] === 105) {
    return 1n;
  }
  return 0n;
}

export { literalEqualsItself, literalsDistinguishCorrectly, gridOfCharsRoundTrip, darrayOfCharsRoundTrip };
