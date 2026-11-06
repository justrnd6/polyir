#ifndef POLYIR_SPARSE_HPP
#define POLYIR_SPARSE_HPP

// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero. `set(value, row, col)` writes one.
template <typename T>
struct Sparse {
  std::vector<int64_t> rows;
  std::vector<int64_t> cols;
  std::vector<T> values;
  T zero;

  explicit Sparse(const T& zero) : zero(zero) {}

  int64_t size() const {
    return values.size();
  }

  T get(int64_t row, int64_t col) const {
    T found = zero;
    for (std::size_t i = 0; i < values.size(); i++) {
      if (rows[i] == row && cols[i] == col) {
        found = values[i];
      }
    }
    return found;
  }

  void set(const T& value, int64_t row, int64_t col) {
    rows.push_back(row);
    cols.push_back(col);
    values.push_back(value);
  }
};
#endif
