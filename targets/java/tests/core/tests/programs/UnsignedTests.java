import java.util.HashMap;
import java.util.Objects;

final class UnsignedTests {
    static double checkAnUnsignedComparisonDoesNotSeeTheTopBitAsASign() {
        final long big = 4000000000L;
        long seen = 0L;
        if (big > 3000000000L) {
            seen = seen + 1L;
        }
        if (big >= big) {
            seen = seen + 10L;
        }
        if (big < 5L) {
            seen = seen + 100L;
        }
        return Check.offInt(seen, 11L);
    }

    static double checkAnUnsignedDivisionAndRemainder() {
        final long big = 4000000000L;
        final long half = big / 2L;
        final long rest = big % 7L;
        return Check.offInt((long) half, 2000000000L) + Check.offInt((long) rest, 3L);
    }

    static double checkAnUnsignedRightShiftFillsWithZeros() {
        final long big = 4000000000L;
        final long shifted = big >> 28L;
        final long m = (long) (0L - 1L);
        return Check.offInt((long) shifted, 14L) + Check.offInt((long) (m >>> 63L), 1L);
    }

    static double checkAnUnsignedWidensWithZeroBits() {
        final long big = 4000000000L;
        final int small = 200;
        return Check.offInt((long) big, 4000000000L) + Check.offInt((long) small, 200L);
    }

    static double checkNarrowUnsignedArithmeticWrapsAtItsSize() {
        final int b = 200;
        final int c = 100;
        final int sum = (b + c) & 0xFF;
        final int under = (c - b) & 0xFF;
        return Check.offInt((long) sum, 44L) + Check.offInt((long) under, 156L);
    }

    static double checkAnUnsignedGoesToAFloatAsItsValue() {
        final long big = 4000000000L;
        final double asDouble = (double) big;
        return Check.offInt((long) (double) asDouble, 4000000000L);
    }

    static double checkTheLargestU64IsLargerThanFive() {
        final long m = (long) (0L - 1L);
        long seen = 0L;
        if (Long.compareUnsigned(m, 5L) > 0) {
            seen = seen + 1L;
        }
        if (Long.divideUnsigned(m, 2L) == 9223372036854775807L) {
            seen = seen + 10L;
        }
        return Check.offInt(seen, 11L);
    }

    record Pixel(int r, int g, int b) {
    }

    static double checkAnUnsignedFieldKeepsItsValueAndStructsCompare() {
        final Pixel a = new Pixel(250, 128, 7);
        final Pixel b = new Pixel(250, 128, 7);
        long seen = 0L;
        if (Objects.equals(a, b)) {
            seen = seen + 1L;
        }
        seen = seen + (long) a.g;
        return Check.offInt(seen, 129L);
    }

    static double checkUnsignedKeysAreOrdinaryKeys() {
        final HashMap<Long, Long> m = new HashMap<>();
        final long k = 4000000000L;
        m.put(k, 7L);
        m.put(5L, 9L);
        return Check.offInt(m.getOrDefault(k, 0L), 7L) + Check.offInt(m.getOrDefault(5L, 0L), 9L);
    }
}
