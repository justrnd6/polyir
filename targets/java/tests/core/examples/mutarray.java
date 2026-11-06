import java.util.ArrayList;

final class mutarray {
    static long sumPushed(long a, long b, long c) {
        final ArrayList<Long> arr = new ArrayList<>();
        arr.add(a);
        arr.add(b);
        arr.add(c);
        arr.add(a + b + c);
        return arr.get(0) + arr.get(1) + arr.get(2) + arr.get(3);
    }

    static long lenCheck(long a, long b, long c) {
        final ArrayList<Long> arr = new ArrayList<>();
        arr.add(a);
        arr.add(b);
        arr.add(c);
        return (long) arr.size();
    }

    static long sumSquaresUpTo(long n) {
        final ArrayList<Long> arr = new ArrayList<>();
        for (long i = 0L; i < n; i = i + 1L) {
            arr.add(i * i);
        }
        long total = 0L;
        for (long i = 0L; i < (long) arr.size(); i = i + 1L) {
            total = total + arr.get((int) i);
        }
        return total;
    }

    static long sumEvensBelow(long n) {
        final ArrayList<Long> arr = new ArrayList<>();
        for (long i = 0L; i < n; i = i + 1L) {
            if (i % 2L == 0L) {
                arr.add(i);
            }
        }
        long total = 0L;
        for (long i = 0L; i < (long) arr.size(); i = i + 1L) {
            total = total + arr.get((int) i);
        }
        return total;
    }

    static long fillSquares(ArrayList<Long> arr, long count) {
        for (long i = 0L; i < count; i = i + 1L) {
            arr.add(i * i);
        }
        return count;
    }

    static long squaresThroughAHelper(long count) {
        final ArrayList<Long> arr = new ArrayList<>();
        fillSquares(arr, count);
        return (long) arr.size() * 1000L + arr.get((int) (count - 1L));
    }

    static long pushGivesTheArrayBack() {
        ArrayList<Long> arr = new ArrayList<>();
        arr = pushed(arr, 10L);
        arr = pushed(arr, 20L);
        return arr.get(0) + arr.get(1);
    }

    static long sumEvensBelowExplicit(long n) {
        long label = 0L;
        long i = 0L;
        ArrayList<Long> arr = new ArrayList<>();
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    final ArrayList<Long> empty = new ArrayList<>();
                    i = 0L;
                    arr = empty;
                    label = 1L;
                    continue machine;
                }
                case 1: {
                    if (i < n) {
                        label = 2L;
                        continue machine;
                    } else {
                        label = 5L;
                        continue machine;
                    }
                }
                case 2: {
                    if (i % 2L == 0L) {
                        label = 3L;
                        continue machine;
                    } else {
                        label = 4L;
                        continue machine;
                    }
                }
                case 3: {
                    long next_i = i + 1L;
                    ArrayList<Long> next_arr = pushed(arr, i);
                    i = next_i;
                    arr = next_arr;
                    label = 1L;
                    continue machine;
                }
                case 4: {
                    i = i + 1L;
                    label = 1L;
                    continue machine;
                }
                case 5: {
                    long i2 = 0L;
                    long total = 0L;
                    while (i2 < (long) arr.size()) {
                        i2 = i2 + 1L;
                        total = total + arr.get((int) i2);
                    }
                    return total;
                }
            }
        }
    }

    // A push used as a value gives the list, as in polyir (Java's add gives whether it changed the list).
    static <T> ArrayList<T> pushed(ArrayList<T> items, T item) {
        items.add(item);
        return items;
    }
}
