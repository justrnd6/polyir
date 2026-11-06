final class arrays {
    static long histTest() {
        final long[] hist = new long[4];
        hist[0] = 10L;
        hist[1] = 20L;
        hist[2] = 30L;
        hist[3] = 40L;
        return hist[1] + hist[2];
    }

    static long lenCheck() {
        final long[] hist = new long[4];
        return (long) hist.length;
    }

    static long sumFixed(long[] arr) {
        return arr[0] + arr[1] + arr[2] + arr[3];
    }

    static long lenOnParam(long[] arr) {
        return (long) arr.length;
    }

    static long callWithLocal() {
        final long[] hist = new long[4];
        hist[0] = 1L;
        hist[1] = 2L;
        hist[2] = 3L;
        hist[3] = 4L;
        return sumFixed(hist) * 100L + lenOnParam(hist);
    }
}
