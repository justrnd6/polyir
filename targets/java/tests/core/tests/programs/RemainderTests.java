final class RemainderTests {
    static double checkFloatRemainder() {
        final float a = 7.5f;
        final float b = 2.0f;
        return Check.off(a % b, 1.5f);
    }

    static double checkFloatRemainderKeepsTheSignOfTheLeftNumber() {
        final float a = (float) (double) (0.0 - 7.5);
        final float b = 2.0f;
        return Check.off(a % b, (float) (double) (0.0 - 1.5));
    }

    static double checkFloatRemainderIgnoresTheSignOfTheRightNumber() {
        final float a = 7.5f;
        final float b = (float) (double) (0.0 - 2.0);
        return Check.off(a % b, 1.5f);
    }

    static double checkFloatRemainderOfNumbersThatDivideEvenly() {
        final float b = 47.0f;
        final float c = 97.0f;
        return Check.off((float) (double) (2867.0 % (double) b), 0.0f) + Check.off((float) (double) (5969.0 % (double) b), 0.0f) + Check.off((float) (double) (9409.0 % (double) c), 0.0f);
    }

    static double checkFloatRemainderOfZero() {
        final float a = 0.0f;
        final float b = 5.0f;
        return Check.off(a % b, 0.0f);
    }

    static double checkFloatRemainderOfAFractionOfOne() {
        final float a = 0.75f;
        final float b = 1.0f;
        return Check.off(a % b, 0.75f);
    }

    static double checkWholeNumberRemainder() {
        final long a = 17L;
        final long b = 5L;
        return Check.offInt(a % b, 2L) + Check.offInt((0L - a) % b, 0L - 2L);
    }
}
