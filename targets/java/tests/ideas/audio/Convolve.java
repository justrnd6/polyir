final class Convolve {
    static float convolveSample(int i, float[] x, float[] h, int[] p) {
        final int nx = p[0];
        final int nh = p[1];
        int first = i - (nx - 1);
        if (first < 0) {
            first = 0;
        }
        int last = i;
        if (last > nh - 1) {
            last = nh - 1;
        }
        float sum = 0.0f;
        for (int j = first; j <= last; j = j + 1) {
            sum = sum + x[i - j] * h[j];
        }
        return sum;
    }
}
