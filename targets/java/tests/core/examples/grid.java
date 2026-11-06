import java.util.Arrays;

final class grid {
    static long fillAndWeight(long scale) {
        final Grid<Long> g = new Grid<>(new int[] {3, 4}, 0L);
        for (long r = 0L; r < 3L; r = r + 1L) {
            for (long c = 0L; c < 4L; c = c + 1L) {
                g.set((r * 4L + c) * scale, r, c);
            }
        }
        long total = 0L;
        for (long r = 0L; r < 3L; r = r + 1L) {
            for (long c = 0L; c < 4L; c = c + 1L) {
                total = total + g.get(r, c) * (r + 1L);
            }
        }
        return total;
    }

    static long singleCellRoundTrip() {
        final Grid<Long> g = new Grid<>(new int[] {2, 2}, 0L);
        g.set(10L, 0L, 0L);
        g.set(20L, 0L, 1L);
        g.set(30L, 1L, 0L);
        g.set(40L, 1L, 1L);
        return g.get(1L, 0L) - g.get(0L, 1L);
    }

    static long dimsCheck() {
        final Grid<Long> g = new Grid<>(new int[] {3, 4}, 0L);
        return (long) g.dims[0] * 100L + (long) g.dims[1];
    }

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
}
