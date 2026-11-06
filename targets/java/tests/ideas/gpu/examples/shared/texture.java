final class texture {
    static float floorf(float x) { return (float) Math.floor(x); }

    static int wrap(int n, int size, int mode) {
        if (mode == 0) {
            if (n < 0) {
                return 0;
            }
            if (n > size - 1) {
                return size - 1;
            }
            return n;
        }
        if (mode == 1) {
            return (n % size + size) % size;
        }
        final int period = size * 2;
        final int m = (n % period + period) % period;
        if (m >= size) {
            return period - 1 - m;
        }
        return m;
    }

    static float texel(float[] tex, int offset, int size, int x, int y, int mode) {
        return tex[offset + wrap(y, size, mode) * size + wrap(x, size, mode)];
    }

    static float nearest(float[] tex, int offset, int size, float u, float v, int mode) {
        final int x = (int) (float) floorf(u * (float) size);
        final int y = (int) (float) floorf(v * (float) size);
        return texel(tex, offset, size, x, y, mode);
    }

    static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    static float bilinear(float[] tex, int offset, int size, float u, float v, int mode) {
        final float sx = u * (float) size - 0.5f;
        final float sy = v * (float) size - 0.5f;
        final int x0 = (int) (float) floorf(sx);
        final int y0 = (int) (float) floorf(sy);
        final float fx = sx - (float) x0;
        final float fy = sy - (float) y0;
        final float top = lerp(texel(tex, offset, size, x0, y0, mode), texel(tex, offset, size, x0 + 1, y0, mode), fx);
        final float bottom = lerp(texel(tex, offset, size, x0, y0 + 1, mode), texel(tex, offset, size, x0 + 1, y0 + 1, mode), fx);
        return lerp(top, bottom, fy);
    }

    static int levelOffset(int base, int level) {
        int offset = 0;
        int size = base;
        for (long k = 0L; k < (long) level; k = k + 1L) {
            offset = offset + size * size;
            size = size / 2;
        }
        return offset;
    }

    static int levelSize(int base, int level) {
        int size = base;
        for (long k = 0L; k < (long) level; k = k + 1L) {
            size = size / 2;
        }
        return size;
    }

    static float mipSample(float[] tex, int base, int levels, float u, float v, float level, int mode) {
        int lo = (int) (float) floorf(level);
        if (lo < 0) {
            lo = 0;
        }
        if (lo > levels - 1) {
            lo = levels - 1;
        }
        int hi = lo + 1;
        if (hi > levels - 1) {
            hi = levels - 1;
        }
        float amount = level - (float) lo;
        if (amount < 0.0f) {
            amount = 0.0f;
        }
        if (amount > 1.0f) {
            amount = 1.0f;
        }
        final float a = bilinear(tex, levelOffset(base, lo), levelSize(base, lo), u, v, mode);
        final float b = bilinear(tex, levelOffset(base, hi), levelSize(base, hi), u, v, mode);
        return lerp(a, b, amount);
    }

    static long pyramidSize(long base) {
        long total = 0L;
        long size = base;
        for (long k = 0L; k < 16L; k = k + 1L) {
            if (size >= 1L) {
                total = total + size * size;
                size = size / 2L;
            }
        }
        return total;
    }

    static long fillBase(float[] tex, long base) {
        for (long y = 0L; y < base; y = y + 1L) {
            for (long x = 0L; x < base; x = x + 1L) {
                float level = (x / 4L + y / 4L) % 2L == 1L ? 0.45f : 0.85f;
                if (x % 4L == 0L || y % 4L == 0L) {
                    level = level * 0.6f;
                }
                tex[(int) (y * base + x)] = level;
            }
        }
        return 0L;
    }

    static long buildPyramid(float[] tex, long base) {
        fillBase(tex, base);
        long from = 0L;
        long size = base;
        for (long level = 0L; level < 16L; level = level + 1L) {
            if (size > 1L) {
                shrink(tex, from, size);
                from = from + size * size;
                size = size / 2L;
            }
        }
        return 0L;
    }

    static long shrink(float[] tex, long from, long size) {
        final long next = from + size * size;
        final long half = size / 2L;
        for (long y = 0L; y < half; y = y + 1L) {
            for (long x = 0L; x < half; x = x + 1L) {
                final float a = tex[(int) (from + 2L * y * size + 2L * x)];
                final float b = tex[(int) (from + 2L * y * size + 2L * x + 1L)];
                final float c = tex[(int) (from + (2L * y + 1L) * size + 2L * x)];
                final float d = tex[(int) (from + (2L * y + 1L) * size + 2L * x + 1L)];
                tex[(int) (next + y * half + x)] = (a + b + c + d) * 0.25f;
            }
        }
        return 0L;
    }
}
