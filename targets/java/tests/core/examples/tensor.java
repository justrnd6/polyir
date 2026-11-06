import java.util.Arrays;

final class tensor {
    static long weightedSum3D() {
        final Grid<Long> t = new Grid<>(new int[] {2, 3, 4}, 0L);
        for (long i = 0L; i < 2L; i = i + 1L) {
            for (long j = 0L; j < 3L; j = j + 1L) {
                for (long k = 0L; k < 4L; k = k + 1L) {
                    t.set(i * 100L + j * 10L + k, i, j, k);
                }
            }
        }
        long total = 0L;
        for (long i = 0L; i < 2L; i = i + 1L) {
            for (long j = 0L; j < 3L; j = j + 1L) {
                for (long k = 0L; k < 4L; k = k + 1L) {
                    total = total + t.get(i, j, k) * (i + 1L) * (j + 1L);
                }
            }
        }
        return total;
    }

    static long cornerRoundTrip() {
        final Grid<Long> t = new Grid<>(new int[] {2, 2, 2}, 0L);
        t.set(1L, 0L, 0L, 0L);
        t.set(2L, 0L, 0L, 1L);
        t.set(3L, 0L, 1L, 0L);
        t.set(4L, 1L, 0L, 0L);
        t.set(5L, 1L, 1L, 1L);
        return t.get(1L, 1L, 1L) * 100L + t.get(1L, 0L, 0L) * 10L + t.get(0L, 1L, 0L);
    }

    static long dimsCheck() {
        final Grid<Long> t = new Grid<>(new int[] {2, 3, 4}, 0L);
        return (long) t.dims[0] * 100L + (long) t.dims[1] * 10L + (long) t.dims[2];
    }

    static long oneDimensional() {
        final Grid<Long> t = new Grid<>(new int[] {4}, 0L);
        t.set(10L, 0L);
        t.set(20L, 1L);
        t.set(30L, 2L);
        t.set(40L, 3L);
        return t.get(1L) + t.get(2L);
    }

    static long fourDimensional() {
        final Grid<Long> t = new Grid<>(new int[] {2, 2, 2, 2}, 0L);
        t.set(1L, 0L, 0L, 0L, 0L);
        t.set(99L, 1L, 1L, 1L, 1L);
        t.set(5L, 0L, 1L, 0L, 1L);
        return t.get(1L, 1L, 1L, 1L) * 100L + t.get(0L, 1L, 0L, 1L);
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
