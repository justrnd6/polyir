import java.util.ArrayList;
import java.util.Arrays;

final class chars {
    static long literalEqualsItself() {
        final byte c = (byte) 65;
        if (c == (byte) 65) {
            return 1L;
        }
        return 0L;
    }

    static long literalsDistinguishCorrectly() {
        final byte a = (byte) 97;
        final byte b = (byte) 98;
        if (a == b) {
            return 0L;
        }
        return 1L;
    }

    static long gridOfCharsRoundTrip() {
        final Grid<Byte> g = new Grid<>(new int[] {1, 3}, (byte) 0);
        g.set((byte) 120, 0L, 0L);
        g.set((byte) 121, 0L, 1L);
        g.set((byte) 122, 0L, 2L);
        if (g.get(0L, 0L) == (byte) 120 && g.get(0L, 1L) == (byte) 121 && g.get(0L, 2L) == (byte) 122) {
            return 1L;
        }
        return 0L;
    }

    static long darrayOfCharsRoundTrip() {
        final ArrayList<Byte> cs = new ArrayList<>();
        cs.add((byte) 104);
        cs.add((byte) 105);
        if (cs.get(0) == (byte) 104 && cs.get(1) == (byte) 105) {
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
