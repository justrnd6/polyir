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
