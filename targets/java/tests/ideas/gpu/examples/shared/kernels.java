final class kernels {
    static float sqrtf(float x) { return (float) Math.sqrt(x); }

    static float scale(float x, float k) {
        return x * k + 1.0f;
    }

    static float norm(float a, float b) {
        final float s = scale(a, 2.0f) + b * b;
        return sqrtf(s);
    }

    static int sumTo(int n) {
        int acc = 0;
        for (long i = 0L; i < (long) n; i = i + 1L) {
            if (i % 2L == 0L) {
                acc = acc + (int) (long) i;
            }
        }
        return acc;
    }

    static float lane(float[] v) {
        return v[0] + v[3];
    }

    static float smooth(int i, float[] src, int n) {
        if (i == 0) {
            return src[i];
        }
        if (i == n - 1) {
            return src[i];
        }
        return (src[i - 1] + src[i] + src[i + 1]) / 3.0f;
    }

    static float leftover(float a, float b) {
        return a % b;
    }

    static long close(float a, float b) {
        float diff = a - b;
        if (diff < 0.0f) {
            diff = (float) (double) (0.0 - (double) diff);
        }
        float size = b;
        if (size < 0.0f) {
            size = (float) (double) (0.0 - (double) size);
        }
        if (diff <= (float) (double) (0.000001 * (double) (size + 1.0f))) {
            return 1L;
        }
        return 0L;
    }
}
