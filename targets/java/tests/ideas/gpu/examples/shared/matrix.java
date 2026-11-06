final class matrix {
    static float[] transform(float[] p, float[] m) {
        return new float[] {m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3] * p[3], m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7] * p[3], m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11] * p[3], m[12] * p[0] + m[13] * p[1] + m[14] * p[2] + m[15] * p[3]};
    }

    static long identity(float[] m) {
        for (long k = 0L; k < 16L; k = k + 1L) {
            m[(int) k] = 0.0f;
        }
        m[0] = 1.0f;
        m[5] = 1.0f;
        m[10] = 1.0f;
        m[15] = 1.0f;
        return 0L;
    }

    static long multiply4(float[] a, float[] b, float[] out) {
        for (long row = 0L; row < 4L; row = row + 1L) {
            for (long col = 0L; col < 4L; col = col + 1L) {
                float sum = 0.0f;
                for (long k = 0L; k < 4L; k = k + 1L) {
                    sum = sum + a[(int) (row * 4L + k)] * b[(int) (k * 4L + col)];
                }
                out[(int) (row * 4L + col)] = sum;
            }
        }
        return 0L;
    }
}
