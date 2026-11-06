final class Wave {
    static float sinf(float x) { return (float) Math.sin(x); }
    static float fabsf(float x) { return Math.abs(x); }
    static float powf(float x, float y) { return (float) Math.pow(x, y); }

    static float sine(float phase) {
        return sinf((float) (double) (6.2831853 * (double) Math_.fract(phase)));
    }

    static float square(float phase) {
        if (Math_.fract(phase) < 0.5f) {
            return 1.0f;
        }
        return (float) (double) (0.0 - 1.0);
    }

    static float sawtooth(float phase) {
        return (float) (double) (2.0 * (double) Math_.fract(phase) - 1.0);
    }

    static float triangle(float phase) {
        return (float) (double) (1.0 - 4.0 * (double) fabsf(Math_.fract(phase) - 0.5f));
    }

    static float noise(int seed) {
        final int a = (seed * 75 + 74) % 65537;
        final int b = (a * 75 + 74) % 65537;
        return (float) (double) (2.0 * (double) (float) b / 65537.0 - 1.0);
    }

    static float fade(float t, float attack, float release, float total) {
        float level = t < attack ? t / attack : 1.0f;
        if (total - t < release) {
            level = level * Math_.clamp((total - t) / release, 0.0f, 1.0f);
        }
        return level;
    }

    static float noteFrequency(float m) {
        return (float) (double) (440.0 * (double) powf(2.0f, (m - 69.0f) / 12.0f));
    }
}
