final class Gpu {
    static int atomicAdd(int[] counts, int slot, int amount) {
        final int old = counts[slot];
        counts[slot] = old + amount;
        return old;
    }

    static float atomicAddFloat(float[] sums, int slot, float amount) {
        final float old = sums[slot];
        sums[slot] = old + amount;
        return old;
    }

    static int atomicMin(int[] values, int slot, int candidate) {
        final int old = values[slot];
        if (candidate < old) {
            values[slot] = candidate;
        }
        return old;
    }

    static int atomicMax(int[] values, int slot, int candidate) {
        final int old = values[slot];
        if (candidate > old) {
            values[slot] = candidate;
        }
        return old;
    }

    static int groupSize() {
        return 1;
    }

    static int localIndex(int i) {
        return 0;
    }

    static int groupIndex(int i) {
        return i;
    }

    static int barrier() {
        return 0;
    }
}
