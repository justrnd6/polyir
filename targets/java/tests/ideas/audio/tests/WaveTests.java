final class WaveTests {
    static double checkSignOfNegative() {
        return Check.off(Math_.sign((float) (double) (0.0 - 3.0)), (float) (double) (0.0 - 1.0));
    }

    static double checkSignOfZero() {
        return Check.off(Math_.sign(0.0f), 0.0f);
    }

    static double checkSignOfPositive() {
        return Check.off(Math_.sign(2.0f), 1.0f);
    }

    static double checkFractOfPositive() {
        return Check.off(Math_.fract(2.75f), 0.75f);
    }

    static double checkFractOfNegative() {
        return Check.off(Math_.fract((float) (double) (0.0 - 0.25)), 0.75f);
    }

    static double checkClampAbove() {
        return Check.off(Math_.clamp(5.0f, (float) (double) (0.0 - 1.0), 2.0f), 2.0f);
    }

    static double checkClampBelow() {
        return Check.off(Math_.clamp((float) (double) (0.0 - 5.0), (float) (double) (0.0 - 1.0), 2.0f), (float) (double) (0.0 - 1.0));
    }

    static double checkMix() {
        return Check.off(Math_.mix(10.0f, 20.0f, 0.25f), 12.5f);
    }

    static double checkSinePeak() {
        return Check.off(Wave.sine(0.25f), 1.0f);
    }

    static double checkSineTrough() {
        return Check.off(Wave.sine(0.75f), (float) (double) (0.0 - 1.0));
    }

    static double checkSineZeroCrossing() {
        return Check.off(Wave.sine(1.5f), 0.0f);
    }

    static double checkSquareHigh() {
        return Check.off(Wave.square(0.25f), 1.0f);
    }

    static double checkSquareLow() {
        return Check.off(Wave.square(0.75f), (float) (double) (0.0 - 1.0));
    }

    static double checkSawtoothStart() {
        return Check.off(Wave.sawtooth(0.0f), (float) (double) (0.0 - 1.0));
    }

    static double checkSawtoothThreeQuarters() {
        return Check.off(Wave.sawtooth(0.75f), 0.5f);
    }

    static double checkTriangleStart() {
        return Check.off(Wave.triangle(0.0f), (float) (double) (0.0 - 1.0));
    }

    static double checkTrianglePeak() {
        return Check.off(Wave.triangle(0.5f), 1.0f);
    }

    static double checkTriangleMiddle() {
        return Check.off(Wave.triangle(0.25f), 0.0f);
    }

    static double checkConcertA() {
        return Check.off(Wave.noteFrequency(69.0f), 440.0f);
    }

    static double checkOctaveAboveConcertA() {
        return Check.off(Wave.noteFrequency(81.0f), 880.0f);
    }

    static double checkFadeInHalfway() {
        return Check.off(Wave.fade(0.5f, 1.0f, 1.0f, 4.0f), 0.5f);
    }

    static double checkFadeHolds() {
        return Check.off(Wave.fade(2.0f, 1.0f, 1.0f, 4.0f), 1.0f);
    }

    static double checkFadeOutHalfway() {
        return Check.off(Wave.fade(3.5f, 1.0f, 1.0f, 4.0f), 0.5f);
    }

    static double checkNoiseRange() {
        long outside = 0L;
        for (long seed = 0L; seed < 1000L; seed = seed + 1L) {
            if (Wave.noise((int) (long) seed) > 1.0f || Wave.noise((int) (long) seed) < (float) (double) (0.0 - 1.0)) {
                outside = outside + 1L;
            }
        }
        return Check.offInt(outside, 0L);
    }

    static double checkNoiseRepeats() {
        return Check.off(Wave.noise(42) - Wave.noise(42), 0.0f);
    }

    static double checkNoiseVaries() {
        if (Wave.noise(1) == Wave.noise(2) && Wave.noise(2) == Wave.noise(3)) {
            return 1.0;
        }
        return 0.0;
    }
}
