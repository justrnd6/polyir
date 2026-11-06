import java.util.ArrayList;

final class methods {
    static long square(long x) {
        return x * x;
    }

    static long sub(long a, long b) {
        return a - b;
    }

    static long equivalenceCheck(long x) {
        return square(x) - square(x);
    }

    static long orderingCheck(long a, long b) {
        return sub(a, b) - (a - b);
    }

    static long pushViaMethodPersists() {
        final ArrayList<Long> arr = new ArrayList<>();
        arr.add(10L);
        arr.add(20L);
        return (long) arr.size();
    }

    static long pushViaMethodSum() {
        final ArrayList<Long> arr = new ArrayList<>();
        arr.add(1L);
        arr.add(2L);
        arr.add(3L);
        return arr.get(0) + arr.get(1) + arr.get(2);
    }
}
