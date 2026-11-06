// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero. `set(value, row, col)` writes one.
static final class Sparse<T> {
    private final ArrayList<Long> rows = new ArrayList<>();
    private final ArrayList<Long> cols = new ArrayList<>();
    private final ArrayList<T> values = new ArrayList<>();
    private final T zero;

    Sparse(T zero) {
        this.zero = zero;
    }

    long size() {
        return values.size();
    }

    T get(long row, long col) {
        T found = zero;
        for (int i = 0; i < values.size(); i++) {
            if (rows.get(i) == row && cols.get(i) == col) {
                found = values.get(i);
            }
        }
        return found;
    }

    void set(T value, long row, long col) {
        rows.add(row);
        cols.add(col);
        values.add(value);
    }

    Sparse<T> copy() {
        final Sparse<T> out = new Sparse<>(zero);
        out.rows.addAll(rows);
        out.cols.addAll(cols);
        out.values.addAll(values);
        return out;
    }
}
