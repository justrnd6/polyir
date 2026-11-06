final class control_flow {
    static long classify(long x) {
        return x < 0L ? 0L - 1L : x == 0L ? 0L : 1L;
    }

    static long maxVal(long a, long b) {
        long result = 0L;
        if (a < b) {
            result = b;
        } else {
            result = a;
        }
        return result;
    }

    static long absViaIf(long x) {
        long r = x;
        if (r < 0L) {
            r = 0L - r;
        }
        return r;
    }
}
