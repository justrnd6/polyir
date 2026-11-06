final class PreludeTests {
    static float floorf(float x) { return (float) Math.floor(x); }

    static float sign(float x) {
        if (x > 0.0f) {
            return 1.0f;
        }
        if (x < 0.0f) {
            return (float) (double) (0.0 - 1.0);
        }
        return 0.0f;
    }

    static float fract(float x) {
        return x - floorf(x);
    }

    static float clamp(float x, float low, float high) {
        if (x < low) {
            return low;
        }
        if (x > high) {
            return high;
        }
        return x;
    }

    static float mix(float a, float b, float t) {
        return a + (b - a) * t;
    }

    static double checkSign() {
        return Check.off(sign((float) (double) (0.0 - 3.0)), (float) (double) (0.0 - 1.0)) + Check.off(sign(0.0f), 0.0f) + Check.off(sign(2.0f), 1.0f);
    }

    static double checkFract() {
        return Check.off(fract(2.75f), 0.75f) + Check.off(fract((float) (double) (0.0 - 0.25)), 0.75f);
    }

    static double checkClamp() {
        return Check.off(clamp(5.0f, (float) (double) (0.0 - 1.0), 2.0f), 2.0f) + Check.off(clamp((float) (double) (0.0 - 5.0), (float) (double) (0.0 - 1.0), 2.0f), (float) (double) (0.0 - 1.0));
    }

    static double checkMix() {
        return Check.off(mix(10.0f, 20.0f, 0.25f), 12.5f);
    }

    static double checkNestedCalls() {
        return Check.off(clamp(fract(1.75f) * 2.0f, (float) (double) (0.0 - 1.0), 1.0f), 1.0f) + Check.off(mix(sign(2.0f), fract(0.5f), 0.5f), 0.75f);
    }

    static float rescale(float x) {
        return x * 3.0f;
    }

    static double checkOwnFunctionsStillWork() {
        return Check.off(rescale(fract(1.25f)), 0.75f);
    }
}
