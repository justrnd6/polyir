final class exceptions {
    static long riskyDouble(long x) {
        if (x < 0L) {
            throw new Thrown(99L);
        }
        return x * 2L;
    }

    static long safeDouble(long x) {
        long result = 0L;
        try {
            result = riskyDouble(x);
        } catch (Thrown thrown) {
            final long code = thrown.value;
            result = code;
        }
        return result;
    }

    static long nestedTryCheck(long x) {
        long result = 0L;
        try {
            try {
                result = riskyDouble(x);
            } catch (Thrown thrown) {
                final long innerCode = thrown.value;
                result = innerCode + 1L;
                throw new Thrown(result);
            }
        } catch (Thrown thrown) {
            final long outerCode = thrown.value;
            result = outerCode * 10L;
        }
        return result;
    }

    // What `throw` throws: the code thrown.
    static final class Thrown extends RuntimeException {
        final long value;

        Thrown(long value) {
            super("thrown " + value);
            this.value = value;
        }
    }
}
