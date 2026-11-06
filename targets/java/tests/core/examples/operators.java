final class operators {
    static long bitwisePrecedenceCheck() {
        final long a = 6L;
        final long b = 3L;
        final long c = 1L;
        final long d = 2L;
        return a | b & c ^ d;
    }

    static long shiftPrecedenceCheck() {
        final long x = 1L;
        return x << 2L & 3L;
    }
}
