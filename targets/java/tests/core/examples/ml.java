import java.util.Arrays;

final class ml {
    static double relu(double x) {
        if (x < 0.0) {
            return 0.0;
        }
        return x;
    }

    static double xorForward(double x1, double x2) {
        final Grid<Double> w1 = new Grid<>(new int[] {2, 2}, 0.0);
        w1.set(1.0, 0L, 0L);
        w1.set(1.0, 0L, 1L);
        w1.set(1.0, 1L, 0L);
        w1.set(1.0, 1L, 1L);
        final Grid<Double> b1 = new Grid<>(new int[] {2}, 0.0);
        b1.set(0.0, 0L);
        b1.set(0.0 - 1.0, 1L);
        final Grid<Double> hidden = new Grid<>(new int[] {2}, 0.0);
        for (long i = 0L; i < 2L; i = i + 1L) {
            double preActivation = b1.get(i);
            preActivation = preActivation + w1.get(i, 0L) * x1;
            preActivation = preActivation + w1.get(i, 1L) * x2;
            hidden.set(relu(preActivation), i);
        }
        final Grid<Double> w2 = new Grid<>(new int[] {2}, 0.0);
        w2.set(1.0, 0L);
        w2.set(0.0 - 2.0, 1L);
        final double b2 = 0.0;
        double output = b2;
        for (long i = 0L; i < 2L; i = i + 1L) {
            output = output + w2.get(i) * hidden.get(i);
        }
        return output;
    }

    static long xorPredict(double x1, double x2) {
        if (xorForward(x1, x2) >= 0.5) {
            return 1L;
        }
        return 0L;
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
