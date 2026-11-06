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
