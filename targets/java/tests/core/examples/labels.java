final class labels {
    static long gcd(long a0, long b0) {
        long label = 0L;
        long a = 0L;
        long b = 0L;
        long result = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    a = a0;
                    b = b0;
                    label = 1L;
                    continue machine;
                }
                case 1: {
                    if (b == 0L) {
                        result = a;
                        label = 3L;
                        continue machine;
                    } else {
                        label = 2L;
                        continue machine;
                    }
                }
                case 2: {
                    final long r = a % b;
                    a = b;
                    b = r;
                    label = 1L;
                    continue machine;
                }
                case 3: {
                    return result;
                }
            }
        }
    }
}
