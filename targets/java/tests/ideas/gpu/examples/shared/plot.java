final class plot {
    static long drawStems(float[] pic, long w, long h, float[] values, long n, float low, float high) {
        for (long k = 0L; k < w * h; k = k + 1L) {
            pic[(int) k] = 0.96f;
        }
        final long zero = (long) (float) (high / (high - low) * (float) (long) h);
        for (long x = 0L; x < w; x = x + 1L) {
            pic[(int) (zero * w + x)] = 0.55f;
        }
        for (long j = 0L; j < n; j = j + 1L) {
            final long column = (long) (float) (((float) (long) j + 0.5f) * (float) (long) w / (float) (long) n);
            long top = (long) (float) ((high - values[(int) j]) / (high - low) * (float) (long) (h - 1L));
            if (top < 0L) {
                top = 0L;
            }
            if (top > h - 1L) {
                top = h - 1L;
            }
            final long from = minOf(top, zero);
            final long to = maxOf(top, zero);
            for (long y = from; y <= to; y = y + 1L) {
                pic[(int) (y * w + column)] = 0.1f;
            }
            for (long dy = 0L - 1L; dy <= 1L; dy = dy + 1L) {
                for (long dx = 0L - 1L; dx <= 1L; dx = dx + 1L) {
                    final long yy = top + dy;
                    final long xx = column + dx;
                    if (yy >= 0L && yy < h && xx >= 0L && xx < w) {
                        pic[(int) (yy * w + xx)] = 0.1f;
                    }
                }
            }
        }
        return 0L;
    }

    static long minOf(long a, long b) {
        if (a < b) {
            return a;
        }
        return b;
    }

    static long maxOf(long a, long b) {
        if (a > b) {
            return a;
        }
        return b;
    }

    static long drawWave(float[] pic, long w, long h, float[] values, long n, float low, float high) {
        for (long k = 0L; k < w * h; k = k + 1L) {
            pic[(int) k] = 0.96f;
        }
        final long zero = (long) (float) (high / (high - low) * (float) (long) h);
        for (long x = 0L; x < w; x = x + 1L) {
            pic[(int) (zero * w + x)] = 0.55f;
        }
        for (long x = 0L; x < w; x = x + 1L) {
            final long from = x * n / w;
            long upto = (x + 1L) * n / w;
            if (upto <= from) {
                upto = from + 1L;
            }
            float lowest = values[(int) from];
            float highest = values[(int) from];
            for (long j = from; j < upto; j = j + 1L) {
                if (values[(int) j] < lowest) {
                    lowest = values[(int) j];
                }
                if (values[(int) j] > highest) {
                    highest = values[(int) j];
                }
            }
            long top = (long) (float) ((high - highest) / (high - low) * (float) (long) (h - 1L));
            long bottom = (long) (float) ((high - lowest) / (high - low) * (float) (long) (h - 1L));
            if (top < 0L) {
                top = 0L;
            }
            if (bottom > h - 1L) {
                bottom = h - 1L;
            }
            for (long y = top; y <= bottom; y = y + 1L) {
                pic[(int) (y * w + x)] = 0.1f;
            }
        }
        return 0L;
    }

    static long drawCurve(float[] pic, long w, long h, float[] values, long n, float low, float high) {
        for (long k = 0L; k < w * h; k = k + 1L) {
            pic[(int) k] = 0.96f;
        }
        final long zero = (long) (float) (high / (high - low) * (float) (long) h);
        for (long x = 0L; x < w; x = x + 1L) {
            pic[(int) (zero * w + x)] = 0.55f;
        }
        long previous = curveRow(values[0], h, low, high);
        for (long x = 0L; x < w; x = x + 1L) {
            final long row = curveRow(values[(int) (x * n / w)], h, low, high);
            for (long y = minOf(row, previous); y <= maxOf(row, previous); y = y + 1L) {
                pic[(int) (y * w + x)] = 0.1f;
            }
            previous = row;
        }
        return 0L;
    }

    static long curveRow(float value, long h, float low, float high) {
        long row = (long) (float) ((high - value) / (high - low) * (float) (long) (h - 1L));
        if (row < 0L) {
            row = 0L;
        }
        if (row > h - 1L) {
            row = h - 1L;
        }
        return row;
    }
}
