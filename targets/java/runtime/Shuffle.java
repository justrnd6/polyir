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
