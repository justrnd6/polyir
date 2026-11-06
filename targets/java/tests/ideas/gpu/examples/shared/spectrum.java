final class spectrum {
    static float sqrtf(float x) { return (float) Math.sqrt(x); }
    static float sinf(float x) { return (float) Math.sin(x); }
    static float cosf(float x) { return (float) Math.cos(x); }

    static float dft(int k, float[] x, float[] p) {
        final int n = (int) (float) p[0];
        float re = 0.0f;
        float im = 0.0f;
        for (long j = 0L; j < (long) n; j = j + 1L) {
            final float turns = (float) (k * (int) (long) j % n) / (float) n;
            re = re + x[(int) j] * cosf((float) (double) (6.2831853 * (double) turns));
            im = im - x[(int) j] * sinf((float) (double) (6.2831853 * (double) turns));
        }
        return (float) (double) (2.0 * (double) sqrtf(re * re + im * im) / (double) (float) n);
    }
}
