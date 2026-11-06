final class waves {
    static long concertAHeight() {
        final float quarterCycle = (float) (double) (0.25 / (double) Wave.noteFrequency(69.0f));
        return (long) (float) (Wave.sine(Wave.noteFrequency(69.0f) * quarterCycle) * 1000.0f);
    }

    static long softAttack() {
        return (long) (float) (Wave.fade(0.5f, 1.0f, 1.0f, 4.0f) * 1000.0f);
    }

    static float chordSample(float t) {
        return (float) (double) (0.3 * (double) (Wave.sine(Wave.noteFrequency(60.0f) * t) + Wave.sine(Wave.noteFrequency(64.0f) * t) + Wave.sine(Wave.noteFrequency(67.0f) * t)));
    }
}
