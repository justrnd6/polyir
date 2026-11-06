final class scatter {
    static float floorf(float x) { return (float) Math.floor(x); }
    static float sinf(float x) { return (float) Math.sin(x); }
    static float cosf(float x) { return (float) Math.cos(x); }

    static int atomicAdd(int[] counts, int slot, int amount) {
        final int old = counts[slot];
        counts[slot] = old + amount;
        return old;
    }

    static int reverseCopy(int i, float[] src, int n, float[] dst) {
        dst[n - 1 - i] = src[i];
        return 0;
    }

    static int histogram(int i, int[] data, int bins, int[] counts) {
        final int bin = data[i] % bins;
        atomicAdd(counts, bin, 1);
        return 0;
    }

    static int histogramRacy(int i, int[] data, int bins, int[] counts) {
        final int bin = data[i] % bins;
        counts[bin] = counts[bin] + 1;
        return 0;
    }

    static int splat(int i, float[] p, int[] cells) {
        final int w = (int) (float) p[0];
        final int arm = i % 2;
        final float t = (float) (i / 2 % 20000) / 20000.0f;
        final float angle = t * 9.0f + (float) arm * 3.14159265f;
        final float radius = (float) (double) (0.08 + 0.85 * (double) t);
        final int h1 = (i * 75 + 74) % 65537;
        final int h2 = (h1 * 75 + 74) % 65537;
        final float spread = (float) (double) (0.015 + 0.09 * (double) t);
        final float x = radius * cosf(angle) + ((float) h1 / 65537.0f - 0.5f) * spread * 2.0f;
        final float y = radius * sinf(angle) + ((float) h2 / 65537.0f - 0.5f) * spread * 2.0f;
        final int px = (int) (float) floorf((x * 0.5f + 0.5f) * (float) w);
        final int py = (int) (float) floorf((y * 0.5f + 0.5f) * (float) w);
        if (px >= 0 && px < w && py >= 0 && py < w) {
            atomicAdd(cells, py * w + px, 1);
        }
        return 0;
    }
}
