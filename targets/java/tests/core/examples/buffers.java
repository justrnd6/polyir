final class buffers {
    static double sumBuf(double[] buf, long len) {
        long i = 0L;
        double total = 0.0;
        while (i < len) {
            i = i + 1L;
            total = total + buf[(int) i];
        }
        return total;
    }
}
