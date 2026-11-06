final class queensFile {
    static int queens(int t, int[] prefixes, int[] p, int[] stack) {
        final int n = p[0];
        final int depth = p[1];
        final int full = (int) (long) ((1L << (long) n) - 1L);
        final int base = t * (n + 1) * 4;
        stack[base] = prefixes[t * 3];
        stack[base + 1] = prefixes[t * 3 + 1];
        stack[base + 2] = prefixes[t * 3 + 2];
        stack[base + 3] = full & ~(prefixes[t * 3] | prefixes[t * 3 + 1] | prefixes[t * 3 + 2]);
        int level = 0;
        int count = 0;
        for (long step = 0L; level >= 0; step = step + 1L) {
            final int at = base + level * 4;
            final int free = stack[at + 3];
            if (free == 0) {
                level = level - 1;
            } else {
                final int bit = free & (int) (long) (0L - (long) free);
                stack[at + 3] = free ^ bit;
                if (depth + level + 1 == n) {
                    count = count + 1;
                } else {
                    final int nextCols = stack[at] | bit;
                    final int nextLeft = (stack[at + 1] | bit) << 1 & full;
                    final int nextRight = (stack[at + 2] | bit) >> 1;
                    level = level + 1;
                    final int below = base + level * 4;
                    stack[below] = nextCols;
                    stack[below + 1] = nextLeft;
                    stack[below + 2] = nextRight;
                    stack[below + 3] = full & ~(nextCols | nextLeft | nextRight);
                }
            }
        }
        return count;
    }

    static long listPrefixes(long n, long depth, int[] out) {
        long total = 1L;
        long r = 0L;
        while (r < depth) {
            total = total * n;
            r = r + 1L;
        }
        final int full = (int) (long) ((1L << n) - 1L);
        long found = 0L;
        for (long code = 0L; code < total; code = code + 1L) {
            int left = 0;
            int right = 0;
            int cols = 0;
            long ok = 1L;
            long rest = code;
            for (long r2 = 0L; r2 < depth; r2 = r2 + 1L) {
                final long column = rest % n;
                rest = rest / n;
                final int bit = (int) (long) (1L << column);
                if ((bit & (cols | left | right)) != 0) {
                    ok = 0L;
                }
                cols = cols | bit;
                left = (left | bit) << 1 & full;
                right = (right | bit) >> 1;
            }
            if (ok == 1L) {
                out[(int) (found * 3L)] = cols;
                out[(int) (found * 3L + 1L)] = left;
                out[(int) (found * 3L + 2L)] = right;
                found = found + 1L;
            }
        }
        return found;
    }
}
