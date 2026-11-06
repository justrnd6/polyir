// A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(value, i, j)`
// writes one.
static final class Grid<T> {
    final int[] dims;
    private final Object[] cells;

    Grid(int[] dims, T zero) {
        this.dims = dims;
        int count = 1;
        for (final int size : dims) {
            count *= size;
        }
        cells = new Object[count];
        Arrays.fill(cells, zero);
    }

    private int place(long[] indices) {
        int at = 0;
        for (int i = 0; i < dims.length; i++) {
            at = at * dims[i] + (int) indices[i];
        }
        return at;
    }

    @SuppressWarnings("unchecked")
    T get(long... indices) {
        return (T) cells[place(indices)];
    }

    void set(T value, long... indices) {
        cells[place(indices)] = value;
    }

    Grid<T> copy() {
        final Grid<T> out = new Grid<>(dims, null);
        System.arraycopy(cells, 0, out.cells, 0, cells.length);
        return out;
    }
}
