final class reduce {
    static float pairSum(int i, float[] src, int n) {
        final int a = (int) (long) (2L * (long) i);
        float total = src[a];
        if (a + 1 < n) {
            total = total + src[a + 1];
        }
        return total;
    }

    static int pairSumInt(int i, int[] src, int n) {
        final int a = (int) (long) (2L * (long) i);
        int total = src[a];
        if (a + 1 < n) {
            total = total + src[a + 1];
        }
        return total;
    }

    static float pairMax(int i, float[] src, int n) {
        final int a = (int) (long) (2L * (long) i);
        float best = src[a];
        if (a + 1 < n) {
            if (src[a + 1] > best) {
                best = src[a + 1];
            }
        }
        return best;
    }

    static long passCount(long n) {
        long passes = 0L;
        long left = n;
        for (long k = 0L; k < 40L; k = k + 1L) {
            if (left > 1L) {
                left = (left + 1L) / 2L;
                passes = passes + 1L;
            }
        }
        return passes;
    }

    static float cpuSum(float[] values, long n) {
        float total = 0.0f;
        for (long i = 0L; i < n; i = i + 1L) {
            total = total + values[(int) i];
        }
        return total;
    }

    static float cpuMax(float[] values, long n) {
        float best = values[0];
        for (long i = 1L; i < n; i = i + 1L) {
            if (values[(int) i] > best) {
                best = values[(int) i];
            }
        }
        return best;
    }

    static long fillTestData(float[] values, long n) {
        for (long i = 0L; i < n; i = i + 1L) {
            values[(int) i] = (float) (long) (i % 7L);
        }
        values[(int) (n / 3L)] = 1000.0f;
        return 0L;
    }
}
