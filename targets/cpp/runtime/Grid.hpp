#ifndef POLYIR_GRID_HPP
#define POLYIR_GRID_HPP

// A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(value, i, j)`
// writes one.
template <typename T>
struct Grid {
  std::vector<int> dims;
  std::vector<T> cells;

  Grid(std::vector<int> sizes, const T& zero) : dims(std::move(sizes)) {
    std::size_t count = 1;
    for (const int size : dims) {
      count *= size;
    }
    cells.assign(count, zero);
  }

  template <typename... Indices>
  std::size_t place(Indices... indices) const {
    std::size_t at = 0;
    std::size_t i = 0;
    ((at = at * dims[i++] + (std::size_t) indices), ...);
    return at;
  }

  template <typename... Indices>
  T get(Indices... indices) const {
    return cells[place(indices...)];
  }

  template <typename... Indices>
  void set(const T& value, Indices... indices) {
    cells[place(indices...)] = value;
  }
};
#endif
