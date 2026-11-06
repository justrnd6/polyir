final class kernels {
    static float sqrtf(float x) { return (float) Math.sqrt(x); }

    static float scale(float x, float k) {
        return x * k;
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
}
