final class SoundTests {
    static SoundFile.Sound quietTone() {
        final SoundFile.Sound s = SoundFile.soundNew(0.1f, 1000L);
        s.tone(0L, 125.0f, 0.5f, 0.0f, 0.1f);
        return s;
    }

    static double checkNewSoundIsMono() {
        return Check.offInt(SoundFile.soundNew(1.0f, 8000L).channels, 1L);
    }

    static double checkNewSoundLength() {
        return Check.offInt(SoundFile.soundNew(2.5f, 8000L).count, 20000L);
    }

    static double checkSeconds() {
        return Check.off(SoundFile.soundNew(2.5f, 8000L).seconds(), 2.5f);
    }

    static double checkTonePeak() {
        return Check.off(quietTone().peak(), 0.5f);
    }

    static double checkSquareTonePeak() {
        final SoundFile.Sound s = SoundFile.soundNew(0.1f, 1000L);
        s.tone(1L, 125.0f, 0.4f, 0.0f, 0.1f);
        return Check.off(s.peak(), 0.4f);
    }

    static double checkScale() {
        final SoundFile.Sound s = quietTone();
        s.scale(0.5f);
        return Check.off(s.peak(), 0.25f);
    }

    static double checkMixAdds() {
        final SoundFile.Sound a = quietTone();
        a.add(quietTone(), 0.0f, 1.0f);
        return Check.off(a.peak(), 1.0f);
    }

    static double checkNormalize() {
        final SoundFile.Sound s = SoundFile.soundNew(0.1f, 1000L);
        s.tone(0L, 125.0f, 0.3f, 0.0f, 0.1f);
        s.normalize(0.9f);
        return Check.off(s.peak(), 0.9f);
    }

    static double checkDelayedMixKeepsLength() {
        final SoundFile.Sound a = SoundFile.soundNew(1.0f, 1000L);
        a.add(quietTone(), 0.5f, 1.0f);
        return Check.offInt(a.count, 1000L);
    }

    static double checkDelayedMixIsSilentBefore() {
        final SoundFile.Sound a = SoundFile.soundNew(1.0f, 1000L);
        a.add(quietTone(), 0.5f, 1.0f);
        return Check.off(a.samples[499], 0.0f);
    }

    static double checkDelayedMixStartsLater() {
        final SoundFile.Sound a = SoundFile.soundNew(1.0f, 1000L);
        a.add(quietTone(), 0.5f, 1.0f);
        return Check.off(a.samples[525], 0.35355339f);
    }

    static double checkToneFadesIn() {
        final SoundFile.Sound s = SoundFile.soundNew(0.5f, 8000L);
        s.tone(0L, 440.0f, 1.0f, 0.0f, 0.5f);
        return Check.off(s.samples[0], 0.0f);
    }

    static double checkNoiseAmplitude() {
        final SoundFile.Sound s = SoundFile.soundNew(0.1f, 8000L);
        s.noise(0.25f, 7L);
        final float loudest = s.peak();
        if (loudest > 0.25f) {
            return 1.0;
        }
        if (loudest < 0.2f) {
            return 1.0;
        }
        return 0.0;
    }

    static float filterGain(long kind, float hz, float cutoff) {
        final SoundFile.Sound s = SoundFile.soundNew(1.0f, 8000L);
        final float[] data = s.samples;
        for (long i = 0L; i < 8000L; i = i + 1L) {
            data[(int) i] = Wave.sine(hz * (float) (long) i / 8000.0f);
        }
        if (kind == 0L) {
            s.lowPass(cutoff);
        } else {
            s.highPass(cutoff);
        }
        float loudest = 0.0f;
        for (long i = 6000L; i < 8000L; i = i + 1L) {
            float level = data[(int) i];
            if (level < 0.0f) {
                level = (float) (double) (0.0 - (double) level);
            }
            if (level > loudest) {
                loudest = level;
            }
        }
        return loudest;
    }

    static double checkLowPassCutsHighTone() {
        return Check.off(filterGain(0L, 1000.0f, 500.0f), 0.3987f);
    }

    static double checkLowPassKeepsLowTone() {
        return Check.off(filterGain(0L, 200.0f, 500.0f), 0.9031f);
    }

    static double checkHighPassKeepsHighTone() {
        return Check.off(filterGain(1L, 1000.0f, 500.0f), 0.7225f);
    }

    static double checkHighPassCutsLowTone() {
        return Check.off(filterGain(1L, 200.0f, 500.0f), 0.3613f);
    }

    static double checkEchoFirstRepeat() {
        final SoundFile.Sound s = SoundFile.soundNew(0.5f, 1000L);
        s.samples[0] = 1.0f;
        s.echo(0.1f, 0.5f);
        return Check.off(s.samples[100], 0.5f);
    }

    static double checkEchoSecondRepeat() {
        final SoundFile.Sound s = SoundFile.soundNew(0.5f, 1000L);
        s.samples[0] = 1.0f;
        s.echo(0.1f, 0.5f);
        return Check.off(s.samples[200], 0.25f);
    }

    static SoundFile.Sound convolved() {
        final SoundFile.Sound a = SoundFile.soundNew(0.002f, 1000L);
        final SoundFile.Sound r = SoundFile.soundNew(0.003f, 1000L);
        a.samples[0] = 1.0f;
        a.samples[1] = 2.0f;
        r.samples[0] = 1.0f;
        r.samples[2] = 3.0f;
        return a.convolve(r);
    }

    static double checkConvolutionLength() {
        return Check.offInt(convolved().count, 4L);
    }

    static double checkConvolutionValues() {
        final float[] y = convolved().samples;
        return Check.off(y[0], 1.0f) + Check.off(y[1], 2.0f) + Check.off(y[2], 3.0f) + Check.off(y[3], 6.0f);
    }

    static SoundFile.Sound reverbOfImpulse() {
        final SoundFile.Sound s = SoundFile.soundNew(1.0f, 10000L);
        s.samples[0] = 1.0f;
        s.reverb(1.0f);
        return s;
    }

    static double checkReverbStart() {
        return Check.off(reverbOfImpulse().samples[0], 0.20525f);
    }

    static double checkReverbFirstEcho() {
        return Check.off(reverbOfImpulse().samples[297], 0.03924f);
    }
}
