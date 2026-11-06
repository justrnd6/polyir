final class audio {
    static float floorf(float x) { return (float) Math.floor(x); }
    static float sinf(float x) { return (float) Math.sin(x); }

    static float synth(int i, float[] p) {
        final float t = (float) i / p[0];
        final float envelope = Wave.fade(t, p[2], p[3], p[4]);
        final float wobble = p[6] * Wave.sine(p[5] * t);
        float sum = 0.0f;
        for (long k = 0L; k < (long) (int) (float) p[1]; k = k + 1L) {
            final float cycles = p[(int) (8L + 2L * k)] * t;
            sum = sum + p[(int) (9L + 2L * k)] * sinf((float) (double) (6.2831853 * (double) (cycles - floorf(cycles)) + (double) wobble));
        }
        return p[7] * envelope * sum;
    }

    static float squareWave(int i, float[] p) {
        return Math_.sign(Wave.sine(p[1] * (float) i / p[0]));
    }

    static float partialSum(int i, float[] p) {
        final float x = (float) (double) (6.2831853 * (double) (float) (i % (int) (float) p[0]) / (double) p[0]);
        float sum = 0.0f;
        for (long h = 1L; h <= (long) (int) (float) p[2]; h = h + 1L) {
            final float number = (float) (long) h;
            final float wave = sinf(number * x);
            if (p[1] < 0.5f) {
                if (h % 2L == 1L) {
                    sum = sum + wave / number;
                }
            }
            if (p[1] > 0.5f && p[1] < 1.5f) {
                if (h % 2L == 1L) {
                    sum = sum + wave / number;
                }
                if (h % 2L == 0L) {
                    sum = sum - wave / number;
                }
            }
            if (p[1] > 1.5f) {
                if (h % 4L == 1L) {
                    sum = sum + wave / (number * number);
                }
                if (h % 4L == 3L) {
                    sum = sum - wave / (number * number);
                }
            }
        }
        if (p[1] < 0.5f) {
            return (float) (double) (1.2732395 * (double) sum);
        }
        if (p[1] < 1.5f) {
            return (float) (double) (0.6366198 * (double) sum);
        }
        return (float) (double) (0.8105695 * (double) sum);
    }

    static float melody(int i, float[] notes, float[] p) {
        final float t = (float) i / p[0];
        float sum = 0.0f;
        for (long n = 0L; n < (long) (int) (float) p[1]; n = n + 1L) {
            final float start = notes[(int) (3L * n)];
            final float length = notes[(int) (3L * n + 1L)];
            final float local = t - start;
            if (local >= 0.0f && local < length) {
                final float envelope = Wave.fade(local, 0.02f, 0.08f, length);
                final float f = notes[(int) (3L * n + 2L)];
                sum = sum + envelope * (Wave.sine(f * t) + (float) (double) (0.5 * (double) Wave.sine((float) (double) (2.0 * (double) f * (double) t))) + (float) (double) (0.25 * (double) Wave.sine((float) (double) (3.0 * (double) f * (double) t))));
            }
        }
        return p[2] * sum;
    }
}
