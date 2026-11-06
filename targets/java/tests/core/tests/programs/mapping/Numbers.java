final class Numbers {
    static long mix(long h, long v) {
        return (h * 31L + v) % 1000003L;
    }

    static long wide() {
        final long big = 9223372036854775807L;
        final long wrapped = big + 1L;
        final long product = 3037000500L * 3037000500L;
        final long neg = 0L - big;
        final long quotient = (0L - 7L) / 2L;
        final long rest = (0L - 7L) % 3L;
        long h = 7L;
        h = mix(h, (wrapped >> 40L) % 1000L);
        h = mix(h, (product >> 33L) % 1000L);
        h = mix(h, (neg >> 50L) % 1000L);
        h = mix(h, quotient + 100L);
        h = mix(h, rest + 100L);
        h = mix(h, big << 3L >> 60L);
        h = mix(h, 0L - 1L >> 5L + 100L);
        return h;
    }

    static long thirtyTwo() {
        final int big = 2147483647;
        final int wrapped = big + 1;
        final int product = (int) (long) (65536L * 65536L);
        final int other = (int) (long) (123456789L * 987L);
        final int quotient = (int) (long) ((0L - 7L) / 2L);
        final int rest = (int) (long) ((0L - 7L) % 3L);
        long h = 11L;
        h = mix(h, (long) wrapped % 100003L);
        h = mix(h, (long) product + 5L);
        h = mix(h, (long) other % 100003L);
        h = mix(h, (long) quotient + 100L);
        h = mix(h, (long) rest + 100L);
        h = mix(h, (long) (wrapped >> 7) % 100003L);
        h = mix(h, (long) (big << 5) % 100003L);
        return h;
    }

    static long unsigned32() {
        final long top = 4294967295L;
        final long wrapped = (top + 2L) & 0xFFFFFFFFL;
        final long product = (long) (4000000000L * 7L) & 0xFFFFFFFFL;
        final long under = (long) (3L - 5L) & 0xFFFFFFFFL;
        final long shifted = (top << 4L) & 0xFFFFFFFFL;
        long h = 13L;
        h = mix(h, (long) wrapped);
        h = mix(h, (long) product % 100003L);
        h = mix(h, (long) under % 100003L);
        h = mix(h, (long) shifted % 100003L);
        h = mix(h, (long) (top >> 3L) % 100003L);
        h = mix(h, (long) (top / 3L) % 100003L);
        h = mix(h, (long) (top % 1000L));
        return h;
    }

    static long narrow() {
        final byte a = (byte) 127;
        final byte b = (byte) (a + (byte) 1);
        final short c = (short) 32767;
        final short d = (short) (c + (short) 1);
        final int e = 250;
        final int f = (e + 10) & 0xFF;
        final int g = 65535;
        final int hh = (g + 3) & 0xFFFF;
        final byte m = (byte) (long) (0L - 128L);
        final byte n = (byte) (m / (byte) (long) (0L - 1L));
        long h = 17L;
        h = mix(h, (long) b + 200L);
        h = mix(h, (long) d + 40000L);
        h = mix(h, (long) f);
        h = mix(h, (long) hh);
        h = mix(h, (long) n + 200L);
        h = mix(h, (long) (byte) (b >> (byte) 2) + 200L);
        h = mix(h, (long) ((e << 3) & 0xFF));
        return h;
    }

    static long casts() {
        final long big = 5000000000L;
        final int cut = (int) (long) big;
        final byte tiny = (byte) (long) 300L;
        final long uns = (long) (0L - 5L) & 0xFFFFFFFFL;
        final long up = (long) (0L - 1L);
        final long back = (long) up;
        final double f = 3.99;
        final double g = 0.0 - 3.99;
        long h = 19L;
        h = mix(h, (long) cut % 100003L);
        h = mix(h, (long) tiny + 200L);
        h = mix(h, (long) uns % 100003L);
        h = mix(h, (long) (up >>> 60L));
        h = mix(h, back + 5L);
        h = mix(h, (long) (double) f);
        h = mix(h, (long) (double) g + 10L);
        h = mix(h, (long) (double) (f * 2.5));
        return h;
    }

    static long main() {
        long h = 1L;
        h = mix(h, wide());
        h = mix(h, thirtyTwo());
        h = mix(h, unsigned32());
        h = mix(h, narrow());
        h = mix(h, casts());
        return h % 251L;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
