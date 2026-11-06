final class FftGpu {
    static long fftStagesGpu(long device, long stage, long first, long second, long total, long batch, long direction) {
        long source = first;
        long target = second;
        final long scratch = Metal.metalEmptyBuffer(device, batch * (total / 2L) * 4L);
        long n = total;
        long s = 1L;
        for (long pass = 0L; pass < 30L; pass = pass + 1L) {
            if (n > 1L) {
                final int[] settings = new int[4];
                settings[0] = (int) (long) total;
                settings[1] = (int) (long) n;
                settings[2] = (int) (long) s;
                settings[3] = (int) (long) direction;
                if (Metal.metalDispatch(device, stage, batch * (total / 2L), source, Metal.metalBuffer(device, (long) settings, 16L), target, scratch) == 0L) {
                    return 0L;
                }
                final long swap = source;
                source = target;
                target = swap;
                n = n / 2L;
                s = s * 2L;
            }
        }
        return source;
    }
}
