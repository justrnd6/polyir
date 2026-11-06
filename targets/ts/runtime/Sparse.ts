// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero.
class Sparse<T> {
  private rows: bigint[] = [];
  private cols: bigint[] = [];
  private values: T[] = [];

  constructor(private readonly zero: T) {}

  get length(): number {
    return this.values.length;
  }

  get(row: bigint, col: bigint): T {
    let found = this.zero;
    for (let i = 0; i < this.values.length; i++) {
      if (this.rows[i] === row && this.cols[i] === col) {
        found = this.values[i];
      }
    }
    return found;
  }

  set(row: bigint, col: bigint, value: T): void {
    this.rows.push(row);
    this.cols.push(col);
    this.values.push(value);
  }

  copy(): Sparse<T> {
    const out = new Sparse<T>(this.zero);
    out.rows = [...this.rows];
    out.cols = [...this.cols];
    out.values = [...this.values];
    return out;
  }
}
