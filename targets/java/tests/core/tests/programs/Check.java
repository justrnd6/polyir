final class Check {
    static double sin(double x) { return Math.sin(x); }
    static double cos(double x) { return Math.cos(x); }
    static double sqrt(double x) { return Math.sqrt(x); }

    static double off(float got, float expected) {
        return (double) (got - expected);
    }

    static double offInt(long got, long expected) {
        return (double) (got - expected);
    }

    static double offRelative(float got, float expected) {
        return (double) ((got - expected) / expected);
    }

    static float amplitudeAt(float[] data, long start, long count, long rate, float hz) {
        double re = 0.0;
        double im = 0.0;
        for (long k = 0L; k < count; k = k + 1L) {
            final double angle = 6.283185307179586 * (double) hz * (double) (start + k) / (double) rate;
            re = re + (double) data[(int) (start + k)] * cos(angle);
            im = im + (double) data[(int) (start + k)] * sin(angle);
        }
        return (float) (double) (2.0 * sqrt(re * re + im * im) / (double) count);
    }
}
