import java.util.HashMap;
import java.util.Objects;

final class Unsigned {
    record Pixel(int r, int g, int b) {
    }

    static long comparison() {
        final long big = 4000000000L;
        if (big > 3000000000L && big >= big && big < 5L == false) {
            return 1L;
        }
        return 0L;
    }

    static long division() {
        final long big = 4000000000L;
        final long half = big / 2L;
        final long rest = big % 7L;
        if ((long) half == 2000000000L && (long) rest == 3L) {
            return 1L;
        }
        return 0L;
    }

    static long shifting() {
        final long big = 4000000000L;
        final long top = (long) (0L - 1L);
        if ((long) (big >> 28L) == 14L && (long) (top >>> 63L) == 1L) {
            return 1L;
        }
        return 0L;
    }

    static long widening() {
        final long big = 4000000000L;
        final int small = 200;
        if ((long) big == 4000000000L && (long) small == 200L) {
            return 1L;
        }
        return 0L;
    }

    static long wrapping() {
        final int a = 200;
        final int b = 100;
        final int sum = (a + b) & 0xFF;
        final int under = (b - a) & 0xFF;
        final int product = 300;
        final int wide = (product * 300) & 0xFFFF;
        if ((long) sum == 44L && (long) under == 156L && (long) wide == 24464L) {
            return 1L;
        }
        return 0L;
    }

    static long floats() {
        final long big = 4000000000L;
        if ((long) (double) (double) big == 4000000000L) {
            return 1L;
        }
        return 0L;
    }

    static long largest() {
        final long top = (long) (0L - 1L);
        if (Long.compareUnsigned(top, 5L) > 0 && Long.divideUnsigned(top, 2L) == 9223372036854775807L) {
            return 1L;
        }
        return 0L;
    }

    static long fnv(long n) {
        long h = 2166136261L;
        for (long i = 0L; i < n; i = i + 1L) {
            h = ((h ^ (long) i & 0xFFFFFFFFL) * 16777619L) & 0xFFFFFFFFL;
        }
        return h;
    }

    static long hashing() {
        if ((long) fnv(3L) == 581859880L) {
            return 1L;
        }
        return 0L;
    }

    static long fields() {
        final Pixel a = new Pixel(250, 128, 7);
        final Pixel b = new Pixel(250, 128, 7);
        if (Objects.equals(a, b) && (long) a.g == 128L && a.r > a.b) {
            return 1L;
        }
        return 0L;
    }

    static long keys() {
        final HashMap<Long, Long> m = new HashMap<>();
        final long k = 4000000000L;
        m.put(k, 7L);
        m.put(5L, 9L);
        if (m.getOrDefault(k, 0L) == 7L && m.getOrDefault(5L, 0L) == 9L) {
            return 1L;
        }
        return 0L;
    }

    static long main() {
        return comparison() + division() + shifting() + widening() + wrapping() + floats() + largest() + hashing() + fields() + keys();
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
