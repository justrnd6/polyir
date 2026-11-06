final class shuffle {
    static long reverse4(long a, long b, long c) {
        final long[] v = new long[] {a, b, c, 40L};
        final long[] r = shuffle(v, 3, 2, 1, 0);
        return r[0] * 1000L + r[1] * 100L + r[2] * 10L + r[3];
    }

    static long duplicateFirstTwo(long a, long b, long c) {
        final long[] v = new long[] {a, b, c, 4L};
        final long[] r = shuffle(v, 0, 0, 1, 1);
        return r[0] * 1000L + r[1] * 100L + r[2] * 10L + r[3];
    }

    // The lanes of a vector in a new order: lane `i` of the result is lane `lanes[i]` of `v`.
    static long[] shuffle(long[] v, int... lanes) {
        final long[] out = new long[lanes.length];
        for (int i = 0; i < lanes.length; i++) {
            out[i] = v[lanes[i]];
        }
        return out;
    }

    static double[] shuffle(double[] v, int... lanes) {
        final double[] out = new double[lanes.length];
        for (int i = 0; i < lanes.length; i++) {
            out[i] = v[lanes[i]];
        }
        return out;
    }
}
