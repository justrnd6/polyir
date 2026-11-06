final class Flow {
    static long sumTo(long n) {
        long label = 0L;
        long total = 0L;
        long i = 0L;
        long result = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    total = 0L;
                    i = 1L;
                    label = 1L;
                    continue machine;
                }
                case 1: {
                    if (i > n) {
                        result = total;
                        label = 3L;
                        continue machine;
                    } else {
                        label = 2L;
                        continue machine;
                    }
                }
                case 2: {
                    total = total + i;
                    i = i + 1L;
                    label = 1L;
                    continue machine;
                }
                case 3: {
                    return result;
                }
            }
        }
    }

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

    static long collatz(long n) {
        long label = 0L;
        long x = 0L;
        long steps = 0L;
        long count = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    x = n;
                    steps = 0L;
                    label = 1L;
                    continue machine;
                }
                case 1: {
                    if (x == 1L) {
                        count = steps;
                        label = 5L;
                        continue machine;
                    } else {
                        label = 2L;
                        continue machine;
                    }
                }
                case 2: {
                    if (x % 2L == 0L) {
                        label = 3L;
                        continue machine;
                    } else {
                        label = 4L;
                        continue machine;
                    }
                }
                case 3: {
                    x = x / 2L;
                    steps = steps + 1L;
                    label = 1L;
                    continue machine;
                }
                case 4: {
                    x = 3L * x + 1L;
                    steps = steps + 1L;
                    label = 1L;
                    continue machine;
                }
                case 5: {
                    return count;
                }
            }
        }
    }

    static long swapped(long a, long b) {
        long label = 0L;
        long x = 0L;
        long y = 0L;
        long k = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    x = a;
                    y = b;
                    k = 0L;
                    label = 1L;
                    continue machine;
                }
                case 1: {
                    if (k == 3L) {
                        label = 3L;
                        continue machine;
                    } else {
                        label = 2L;
                        continue machine;
                    }
                }
                case 2: {
                    long next_x = y;
                    long next_y = x;
                    long next_k = k + 1L;
                    x = next_x;
                    y = next_y;
                    k = next_k;
                    label = 1L;
                    continue machine;
                }
                case 3: {
                    return x * 10L + y;
                }
            }
        }
    }

    static long weight(long day) {
        long label = 0L;
        long w = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    if (day == 0L) {
                        label = 1L;
                        continue machine;
                    } else if (day == 6L) {
                        label = 1L;
                        continue machine;
                    } else if (day == 1L) {
                        label = 2L;
                        continue machine;
                    } else if (day == 2L) {
                        label = 2L;
                        continue machine;
                    } else {
                        label = 3L;
                        continue machine;
                    }
                }
                case 1: {
                    w = 1L;
                    label = 4L;
                    continue machine;
                }
                case 2: {
                    w = 2L;
                    label = 4L;
                    continue machine;
                }
                case 3: {
                    w = 3L;
                    label = 4L;
                    continue machine;
                }
                case 4: {
                    return w * 100L;
                }
            }
        }
    }

    static long zigzag(long a) {
        long label = 0L;
        long p = 0L;
        long q = 0L;
        long r = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    if (a == 0L) {
                        p = a;
                        label = 1L;
                        continue machine;
                    } else {
                        q = a;
                        label = 2L;
                        continue machine;
                    }
                }
                case 1: {
                    final long next = p + 1L;
                    if (next > 5L) {
                        r = next;
                        label = 3L;
                        continue machine;
                    } else {
                        q = next;
                        label = 2L;
                        continue machine;
                    }
                }
                case 2: {
                    if (q > 5L) {
                        r = q;
                        label = 3L;
                        continue machine;
                    } else {
                        p = q + 1L;
                        label = 1L;
                        continue machine;
                    }
                }
                case 3: {
                    return r * 2L;
                }
            }
        }
    }

    static long main() {
        long score = sumTo(10L);
        score = score + gcd(48L, 18L);
        score = score + collatz(6L);
        score = score + swapped(4L, 7L);
        score = score + weight(0L) + weight(2L) + weight(9L);
        score = score + zigzag(0L) + zigzag(3L);
        return score;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
