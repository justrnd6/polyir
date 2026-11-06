final class BiquadFile {
    static double sin(double x) { return Math.sin(x); }
    static double cos(double x) { return Math.cos(x); }
    static double pow(double x, double y) { return Math.pow(x, y); }
    static double sqrt(double x) { return Math.sqrt(x); }

    record Biquad(double b0, double b1, double b2, double a1, double a2) {
        public long run(float[] data, long count) {
            double x1 = 0.0;
            double x2 = 0.0;
            double y1 = 0.0;
            double y2 = 0.0;
            for (long i = 0L; i < count; i = i + 1L) {
                final double x = (double) data[(int) i];
                final double y = this.b0 * x + this.b1 * x1 + this.b2 * x2 - this.a1 * y1 - this.a2 * y2;
                x2 = x1;
                x1 = x;
                y2 = y1;
                y1 = y;
                data[(int) i] = (float) (double) y;
            }
            return 0L;
        }

        public float gain(float hz, long rate) {
            final double w = 6.283185307179586 * (double) hz / (double) rate;
            final double c1 = cos(w);
            final double s1 = sin(w);
            final double c2 = cos(2.0 * w);
            final double s2 = sin(2.0 * w);
            final double topReal = this.b0 + this.b1 * c1 + this.b2 * c2;
            final double topImag = 0.0 - (this.b1 * s1 + this.b2 * s2);
            final double bottomReal = 1.0 + this.a1 * c1 + this.a2 * c2;
            final double bottomImag = 0.0 - (this.a1 * s1 + this.a2 * s2);
            final double strength = (topReal * topReal + topImag * topImag) / (bottomReal * bottomReal + bottomImag * bottomImag);
            return (float) (double) sqrt(strength);
        }
    }

    static Biquad biquadFrom(double b0, double b1, double b2, double a0, double a1, double a2) {
        return new Biquad(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    static double biquadAngle(long rate, float hz) {
        return 6.283185307179586 * (double) hz / (double) rate;
    }

    static Biquad biquadLowPass(long rate, float hz, float q) {
        final double w = biquadAngle(rate, hz);
        final double c = cos(w);
        final double alpha = sin(w) / (2.0 * (double) q);
        return biquadFrom((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
    }

    static Biquad biquadHighPass(long rate, float hz, float q) {
        final double w = biquadAngle(rate, hz);
        final double c = cos(w);
        final double alpha = sin(w) / (2.0 * (double) q);
        return biquadFrom((1.0 + c) / 2.0, 0.0 - (1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
    }

    static Biquad biquadBandPass(long rate, float hz, float q) {
        final double w = biquadAngle(rate, hz);
        final double c = cos(w);
        final double alpha = sin(w) / (2.0 * (double) q);
        return biquadFrom(alpha, 0.0, 0.0 - alpha, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
    }

    static Biquad biquadNotch(long rate, float hz, float q) {
        final double w = biquadAngle(rate, hz);
        final double c = cos(w);
        final double alpha = sin(w) / (2.0 * (double) q);
        return biquadFrom(1.0, 0.0 - 2.0 * c, 1.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
    }

    static Biquad biquadBell(long rate, float hz, float q, float gainDb) {
        final double w = biquadAngle(rate, hz);
        final double c = cos(w);
        final double alpha = sin(w) / (2.0 * (double) q);
        final double a = pow(10.0, (double) gainDb / 40.0);
        return biquadFrom(1.0 + alpha * a, 0.0 - 2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, 0.0 - 2.0 * c, 1.0 - alpha / a);
    }

    static Biquad biquadLowShelf(long rate, float hz, float gainDb) {
        final double w = biquadAngle(rate, hz);
        final double c = cos(w);
        final double a = pow(10.0, (double) gainDb / 40.0);
        final double beta = sqrt(2.0 * a) * sin(w);
        return biquadFrom(a * (a + 1.0 - (a - 1.0) * c + beta), 2.0 * a * (a - 1.0 - (a + 1.0) * c), a * (a + 1.0 - (a - 1.0) * c - beta), a + 1.0 + (a - 1.0) * c + beta, 0.0 - 2.0 * (a - 1.0 + (a + 1.0) * c), a + 1.0 + (a - 1.0) * c - beta);
    }

    static Biquad biquadHighShelf(long rate, float hz, float gainDb) {
        final double w = biquadAngle(rate, hz);
        final double c = cos(w);
        final double a = pow(10.0, (double) gainDb / 40.0);
        final double beta = sqrt(2.0 * a) * sin(w);
        return biquadFrom(a * (a + 1.0 + (a - 1.0) * c + beta), 0.0 - 2.0 * a * (a - 1.0 + (a + 1.0) * c), a * (a + 1.0 + (a - 1.0) * c - beta), a + 1.0 - (a - 1.0) * c + beta, 2.0 * (a - 1.0 - (a + 1.0) * c), a + 1.0 - (a - 1.0) * c - beta);
    }
}
