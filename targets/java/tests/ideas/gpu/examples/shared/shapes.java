final class shapes {
    static float sinf(float x) { return (float) Math.sin(x); }

    static int atomicAdd(int[] counts, int slot, int amount) {
        final int old = counts[slot];
        counts[slot] = old + amount;
        return old;
    }

    static float atomicAddFloat(float[] sums, int slot, float amount) {
        final float old = sums[slot];
        sums[slot] = old + amount;
        return old;
    }

    static int atomicMin(int[] values, int slot, int candidate) {
        final int old = values[slot];
        if (candidate < old) {
            values[slot] = candidate;
        }
        return old;
    }

    static int atomicMax(int[] values, int slot, int candidate) {
        final int old = values[slot];
        if (candidate > old) {
            values[slot] = candidate;
        }
        return old;
    }

    static float blend7(float a, float b, float c, float d, float e, float f, float g, float k) {
        return k * (a + (float) (double) (2.0 * (double) b) + (float) (double) (3.0 * (double) c)) + (d - e) * (f + g);
    }

    static float plasma(int x, int y, int width, int height, float phase) {
        final float u = (float) x / (float) width;
        final float v = (float) y / (float) height;
        final float a = sinf((float) (double) (10.0 * (double) u + (double) phase));
        final float b = sinf((float) (double) (8.0 * (double) v - (double) phase));
        final float c = sinf((float) (double) (6.0 * (double) (u + v) + 2.0 * (double) phase));
        return (float) (double) (0.5 + 0.1667 * (double) (a + b + c));
    }

    static float vignette(float pixel, int x, int y, int width, int height) {
        final float dx = (float) x / (float) width - 0.5f;
        final float dy = (float) y / (float) height - 0.5f;
        final float edge = (float) (double) (1.0 - 1.6 * (double) (dx * dx + dy * dy));
        return pixel * edge;
    }

    static int tally(int i, int[] counts, float[] sums, int[] lows, int[] highs) {
        final int value = i * 7919 % 10007;
        atomicAdd(counts, i % 4, 1);
        atomicAddFloat(sums, i % 4, 0.5f);
        atomicMin(lows, 0, value);
        atomicMax(highs, 0, value);
        return 0;
    }

    static int reported(int i) {
        return i * 7919 % 10007;
    }

    static long nearly(float a, float b, float tolerance) {
        float gap = a - b;
        if (gap < 0.0f) {
            gap = (float) (double) (0.0 - (double) gap);
        }
        if (gap <= tolerance) {
            return 1L;
        }
        return 0L;
    }
}
