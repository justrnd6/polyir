final class euclid {
    static long gcd(long a, long b) {
        long x = a;
        long y = b;
        while (y != 0L) {
            final long r = x % y;
            x = y;
            y = r;
        }
        return x;
    }

    static long lcm(long a, long b) {
        return a / gcd(a, b) * b;
    }
}
