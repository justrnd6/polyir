import java.util.ArrayList;

final class sparse {
    static long writeAndReadBack() {
        final Sparse<Long> sp = new Sparse<>(0L);
        sp.set(10L, 0L, 0L);
        sp.set(20L, 2L, 3L);
        sp.set(30L, 5L, 1L);
        return sp.get(2L, 3L) * 100L + sp.get(5L, 1L) * 10L + sp.get(0L, 0L);
    }

    static long unsetCellIsZero() {
        final Sparse<Long> sp = new Sparse<>(0L);
        sp.set(99L, 1L, 1L);
        return sp.get(9L, 9L);
    }

    static long lastWriteWins() {
        final Sparse<Long> sp = new Sparse<>(0L);
        sp.set(1L, 3L, 3L);
        sp.set(2L, 3L, 3L);
        sp.set(3L, 3L, 3L);
        return sp.get(3L, 3L);
    }

    static long oneDimensionalRoundTrip() {
        final Sparse<Long> sp = new Sparse<>(0L);
        sp.set(100L, 0L, 0L);
        sp.set(200L, 7L, 0L);
        sp.set(300L, 3L, 0L);
        return sp.get(7L, 0L) * 100L + sp.get(3L, 0L) * 10L + sp.get(0L, 0L);
    }

    static long lenAfterWrites() {
        final Sparse<Long> sp = new Sparse<>(0L);
        sp.set(1L, 0L, 0L);
        sp.set(2L, 1L, 1L);
        sp.set(3L, 1L, 1L);
        return (long) sp.size();
    }

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
}
