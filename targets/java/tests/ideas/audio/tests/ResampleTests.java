final class ResampleTests {
    static double sin(double x) { return Math.sin(x); }

    static SoundFile.Sound sineSound(float hz, float seconds, long rate) {
        final SoundFile.Sound s = SoundFile.soundNew(seconds, rate);
        for (long i = 0L; i < s.count; i = i + 1L) {
            s.samples[(int) i] = (float) (double) sin(6.283185307179586 * (double) hz * (double) i / (double) rate);
        }
        return s;
    }

    static double checkCountGoingUp() {
        return Check.offInt(Resample.resampledCount(44100L, 44100L, 48000L), 48000L);
    }

    static double checkCountGoingDown() {
        return Check.offInt(Resample.resampledCount(1000L, 8000L, 4000L), 500L);
    }

    static double checkCountRounds() {
        return Check.offInt(Resample.resampledCount(10L, 3L, 4L), 13L);
    }

    static double checkSoundKeepsItsLength() {
        final SoundFile.Sound s = sineSound(440.0f, 0.5f, 8000L).resample(12000L);
        return Check.offInt(s.rate, 12000L) + Check.offInt(s.count, 6000L) + Check.offInt(s.channels, 1L) + Check.off(s.seconds(), 0.5f);
    }

    static double checkUpsamplingFollowsTheWave() {
        final SoundFile.Sound s = sineSound(440.0f, 1.0f, 8000L).resample(12000L);
        double worst = 0.0;
        for (long j = 1000L; j < 11000L; j = j + 7L) {
            final double expected = sin(6.283185307179586 * 440.0 * (double) j / 12000.0);
            double gap = (double) s.samples[(int) j] - expected;
            if (gap < 0.0) {
                gap = 0.0 - gap;
            }
            if (gap > worst) {
                worst = gap;
            }
        }
        return worst;
    }

    static double checkDownsamplingRemovesAliases() {
        final SoundFile.Sound s = sineSound(3000.0f, 1.0f, 8000L).resample(4000L);
        return (double) Check.amplitudeAt(s.samples, 500L, 2000L, 4000L, 1000.0f);
    }

    static double checkDownsamplingKeepsLowTones() {
        final SoundFile.Sound s = sineSound(1000.0f, 1.0f, 8000L).resample(4000L);
        return Check.off(Check.amplitudeAt(s.samples, 500L, 2000L, 4000L, 1000.0f), 1.0f);
    }

    static double checkRoundTrip() {
        final SoundFile.Sound original = sineSound(300.0f, 1.0f, 8000L);
        final SoundFile.Sound back = original.resample(12000L).resample(8000L);
        double worst = 0.0;
        for (long j = 1000L; j < 7000L; j = j + 11L) {
            double gap = (double) (back.samples[(int) j] - original.samples[(int) j]);
            if (gap < 0.0) {
                gap = 0.0 - gap;
            }
            if (gap > worst) {
                worst = gap;
            }
        }
        return worst;
    }

    static double checkConstantStaysConstant() {
        final SoundFile.Sound s = SoundFile.soundNew(0.5f, 8000L);
        for (long i = 0L; i < s.count; i = i + 1L) {
            s.samples[(int) i] = 1.0f;
        }
        final SoundFile.Sound up = s.resample(22050L);
        final SoundFile.Sound down = s.resample(3000L);
        return Check.off(up.samples[(int) (up.count / 2L)], 1.0f) + Check.off(down.samples[(int) (down.count / 2L)], 1.0f);
    }

    static double checkStereoResampling() {
        final SoundFile.Sound left = sineSound(440.0f, 0.5f, 8000L);
        final SoundFile.Sound right = sineSound(880.0f, 0.5f, 8000L);
        final SoundFile.Sound out = SoundFile.soundStereo(left, right).resample(16000L);
        return Check.offInt(out.channels, 2L) + Check.offInt(out.count, 8000L) + Check.off(Check.amplitudeAt(out.channel(0L).samples, 2000L, 4000L, 16000L, 440.0f), 1.0f) + Check.off(Check.amplitudeAt(out.channel(1L).samples, 2000L, 4000L, 16000L, 880.0f), 1.0f);
    }

    static double checkSameRateIsACopy() {
        final SoundFile.Sound s = sineSound(440.0f, 0.1f, 8000L);
        final SoundFile.Sound copy = s.resample(8000L);
        double worst = 0.0;
        for (long i = 100L; i < 700L; i = i + 1L) {
            double gap = (double) (copy.samples[(int) i] - s.samples[(int) i]);
            if (gap < 0.0) {
                gap = 0.0 - gap;
            }
            if (gap > worst) {
                worst = gap;
            }
        }
        return worst + Check.offInt(copy.count, s.count);
    }
}
