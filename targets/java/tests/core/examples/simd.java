final class simd {
    static double vecMulSum(double a, double b) {
        final double[] v1 = new double[] {a, a + 1.0, a + 2.0, a + 3.0};
        final double[] v2 = new double[] {b, b, b, b};
        final double[] p = lanes(v1, v2, (x, y) -> x * y);
        return p[0] + p[1] + p[2] + p[3];
    }

    static long sumI64Vec(long a, long b, long c) {
        final long[] v = new long[] {a, b, c};
        return v[0] + v[1] + v[2];
    }

    static double vecMulSumViaVar(double a, double b) {
        final double[] v1 = new double[] {a, a + 1.0, a + 2.0, a + 3.0};
        final double[] v2 = new double[] {b, b, b, b};
        final double[] p = lanes(v1, v2, (x, y) -> x * y);
        double total = 0.0;
        for (long i = 0L; i < 4L; i = i + 1L) {
            total = total + p[(int) i];
        }
        return total;
    }

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
}
