// An operator lane by lane on two vectors.
static double[] lanes(double[] a, double[] b, java.util.function.DoubleBinaryOperator op) {
    final double[] out = new double[a.length];
    for (int i = 0; i < a.length; i++) {
        out[i] = op.applyAsDouble(a[i], b[i]);
    }
    return out;
}

static long[] lanes(long[] a, long[] b, java.util.function.LongBinaryOperator op) {
    final long[] out = new long[a.length];
    for (int i = 0; i < a.length; i++) {
        out[i] = op.applyAsLong(a[i], b[i]);
    }
    return out;
}
