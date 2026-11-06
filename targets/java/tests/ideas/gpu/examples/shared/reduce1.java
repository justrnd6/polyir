final class reduce1 {
    static float fmaxf(float x, float y) { return Math.max(x, y); }

    static float atomicAddFloat(float[] sums, int slot, float amount) {
        final float old = sums[slot];
        sums[slot] = old + amount;
        return old;
    }

    static int groupSize() {
        return 1;
    }

    static int localIndex(int i) {
        return 0;
    }

    static int groupIndex(int i) {
        return i;
    }

    static int barrier() {
        return 0;
    }

    static int blockSum(int i, float[] src, int n, float[] tile, float[] partial) {
        final int lane = localIndex(i);
        if (i < n) {
            tile[lane] = src[i];
        } else {
            tile[lane] = 0.0f;
        }
        barrier();
        for (int s = groupSize() / 2; s > 0; s = s / 2) {
            if (lane < s) {
                tile[lane] = tile[lane] + tile[lane + s];
            }
            barrier();
        }
        if (lane == 0) {
            partial[groupIndex(i)] = tile[0];
        }
        return 0;
    }

    static int blockMax(int i, float[] src, int n, float[] tile, float[] partial) {
        final int lane = localIndex(i);
        if (i < n) {
            tile[lane] = src[i];
        } else {
            tile[lane] = src[0];
        }
        barrier();
        for (int s = groupSize() / 2; s > 0; s = s / 2) {
            if (lane < s) {
                tile[lane] = fmaxf(tile[lane], tile[lane + s]);
            }
            barrier();
        }
        if (lane == 0) {
            partial[groupIndex(i)] = tile[0];
        }
        return 0;
    }

    static int total(int i, float[] src, int n, float[] tile, float[] sum) {
        final int lane = localIndex(i);
        if (i < n) {
            tile[lane] = src[i];
        } else {
            tile[lane] = 0.0f;
        }
        barrier();
        for (int s = groupSize() / 2; s > 0; s = s / 2) {
            if (lane < s) {
                tile[lane] = tile[lane] + tile[lane + s];
            }
            barrier();
        }
        if (lane == 0) {
            atomicAddFloat(sum, 0, tile[0]);
        }
        return 0;
    }

    static long groupsFor(long n) {
        return (n + 255L) / 256L;
    }

    static long launchCount(long n) {
        long launches = 0L;
        long left = n;
        for (long k = 0L; k < 10L; k = k + 1L) {
            if (left > 1L) {
                left = groupsFor(left);
                launches = launches + 1L;
            }
        }
        return launches;
    }
}
