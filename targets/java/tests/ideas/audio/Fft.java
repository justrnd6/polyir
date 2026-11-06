final class Fft {
    static float sqrtf(float x) { return (float) Math.sqrt(x); }
    static float sinf(float x) { return (float) Math.sin(x); }
    static float cosf(float x) { return (float) Math.cos(x); }
    static float logf(float x) { return (float) Math.log(x); }

    static int fftStage(int t, float[] src, int[] p, float[] dst) {
        final int total = p[0];
        final int n = p[1];
        final int s = p[2];
        final int half = total / 2;
        final int batch = t / half;
        final int local = t % half;
        final int position = local / s;
        final int column = local % s;
        final int base = batch * total * 2;
        final int a = base + (int) (long) (2L * (long) (column + s * position));
        final int b = base + (int) (long) (2L * (long) (column + s * (position + n / 2)));
        final float sumRe = src[a] + src[b];
        final float sumIm = src[a + 1] + src[b + 1];
        final float diffRe = src[a] - src[b];
        final float diffIm = src[a + 1] - src[b + 1];
        final float angle = (float) (double) ((0.0 - 6.2831853) * (double) (float) p[3] * (double) (float) position / (double) (float) n);
        final float twiddleRe = cosf(angle);
        final float twiddleIm = sinf(angle);
        final int first = base + (int) (long) (2L * (long) (column + s * (int) (long) (2L * (long) position)));
        final int second = base + (int) (long) (2L * (long) (column + s * (int) (long) (2L * (long) position + 1L)));
        dst[first] = sumRe;
        dst[first + 1] = sumIm;
        dst[second] = diffRe * twiddleRe - diffIm * twiddleIm;
        dst[second + 1] = diffRe * twiddleIm + diffIm * twiddleRe;
        return 0;
    }

    static float fftMagnitude(int k, float[] data, int[] p) {
        final int total = p[0];
        final float re = data[(int) (2L * (long) k)];
        final float im = data[(int) (2L * (long) k + 1L)];
        return (float) (double) (2.0 * (double) sqrtf(re * re + im * im) / (double) (float) total);
    }

    static int fftFrames(int t, float[] signal, int[] p, float[] dst) {
        final int total = p[0];
        final int frame = t / total;
        final int k = t % total;
        final float window = (float) (double) (0.5 - 0.5 * (double) cosf((float) (double) (6.2831853 * (double) (float) k / (double) (float) (total - 1))));
        dst[(int) (2L * (long) t)] = signal[frame * p[1] + k] * window;
        dst[(int) (2L * (long) t + 1L)] = 0.0f;
        return 0;
    }

    static float spectrogramPixel(int i, float[] data, int[] p) {
        final int total = p[0];
        final int frames = p[1];
        final int rows = p[2];
        final int x = i % frames;
        final int bin = rows - 1 - i / frames;
        final int at = x * total * 2 + (int) (long) (2L * (long) bin);
        final float re = data[at];
        final float im = data[at + 1];
        final float magnitude = (float) (double) (4.0 * (double) sqrtf(re * re + im * im) / (double) (float) total);
        float level = (float) (double) ((20.0 * (double) logf(magnitude + 0.0000001f) / 2.3025851 + 70.0) / 70.0);
        if (level < 0.0f) {
            level = 0.0f;
        }
        if (level > 1.0f) {
            level = 1.0f;
        }
        return level;
    }

    static int complexMultiply(int k, float[] a, float[] b, float[] out) {
        out[(int) (2L * (long) k)] = a[(int) (2L * (long) k)] * b[(int) (2L * (long) k)] - a[(int) (2L * (long) k + 1L)] * b[(int) (2L * (long) k + 1L)];
        out[(int) (2L * (long) k + 1L)] = a[(int) (2L * (long) k)] * b[(int) (2L * (long) k + 1L)] + a[(int) (2L * (long) k + 1L)] * b[(int) (2L * (long) k)];
        return 0;
    }

    static float realScaled(int i, float[] data, float[] p) {
        return data[(int) (2L * (long) i)] / p[0];
    }
}
