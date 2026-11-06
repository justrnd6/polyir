final class zbuffer {
    static float fminf(float x, float y) { return Math.min(x, y); }
    static float fmaxf(float x, float y) { return Math.max(x, y); }
    static float floorf(float x) { return (float) Math.floor(x); }
    static float ceilf(float x) { return (float) Math.ceil(x); }

    static int atomicMin(int[] values, int slot, int candidate) {
        final int old = values[slot];
        if (candidate < old) {
            values[slot] = candidate;
        }
        return old;
    }

    static int emptyKey() {
        return 2147483647;
    }

    static int rasterize(int t, float[] tris, int width, int height, int[] depth) {
        final int base = t * 10;
        final float x0 = tris[base];
        final float y0 = tris[base + 1];
        final float z0 = tris[base + 2];
        final float x1 = tris[base + 3];
        final float y1 = tris[base + 4];
        final float z1 = tris[base + 5];
        final float x2 = tris[base + 6];
        final float y2 = tris[base + 7];
        final float z2 = tris[base + 8];
        final float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
        final int left = (int) (float) fmaxf(0.0f, floorf(fminf(x0, fminf(x1, x2))));
        final int right = (int) (float) fminf((float) width - 1.0f, ceilf(fmaxf(x0, fmaxf(x1, x2))));
        final int top = (int) (float) fmaxf(0.0f, floorf(fminf(y0, fminf(y1, y2))));
        final int bottom = (int) (float) fminf((float) height - 1.0f, ceilf(fmaxf(y0, fmaxf(y1, y2))));
        for (int y = top; y <= bottom; y = y + 1) {
            for (int x = left; x <= right; x = x + 1) {
                final float px = (float) x + 0.5f;
                final float py = (float) y + 0.5f;
                final float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area;
                final float w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area;
                final float w2 = (float) (double) (1.0 - (double) w0 - (double) w1);
                if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                    final float z = w0 * z0 + w1 * z1 + w2 * z2;
                    atomicMin(depth, y * width + x, (int) (float) (z * 1048575.0f) * 512 + t);
                }
            }
        }
        return 0;
    }

    static float shade(int p, int[] depth, float[] tris) {
        final int key = depth[p];
        if (key == 2147483647) {
            return 0.06f;
        }
        final int id = key % 512;
        final float z = (float) (key / 512) / 1048575.0f;
        return tris[id * 10 + 9] * (float) (double) (1.0 - 0.45 * (double) z);
    }

    static long makeScene(float[] tris, long count, long width, long height) {
        long seed = 12345L;
        for (long t = 0L; t < count; t = t + 1L) {
            final float cx = nextRandom(seed) * (float) (long) width;
            seed = nextSeed(seed);
            final float cy = nextRandom(seed) * (float) (long) height;
            seed = nextSeed(seed);
            final float size = (float) (double) (30.0 + (double) (nextRandom(seed) * 110.0f));
            seed = nextSeed(seed);
            for (long corner = 0L; corner < 3L; corner = corner + 1L) {
                tris[(int) (t * 10L + corner * 3L)] = cx + (nextRandom(seed) - 0.5f) * 2.0f * size;
                seed = nextSeed(seed);
                tris[(int) (t * 10L + corner * 3L + 1L)] = cy + (nextRandom(seed) - 0.5f) * 2.0f * size;
                seed = nextSeed(seed);
                tris[(int) (t * 10L + corner * 3L + 2L)] = nextRandom(seed);
                seed = nextSeed(seed);
            }
            tris[(int) (t * 10L + 9L)] = (float) (double) (0.25 + 0.75 * (double) nextRandom(seed));
            seed = nextSeed(seed);
        }
        return 0L;
    }

    static long nextSeed(long seed) {
        return (seed * 1103515245L + 12345L) % 2147483648L;
    }

    static float nextRandom(long seed) {
        return (float) (long) (seed / 65536L % 32768L) / 32768.0f;
    }
}
