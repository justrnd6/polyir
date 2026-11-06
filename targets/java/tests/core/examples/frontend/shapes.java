import java.util.ArrayList;
import java.util.Arrays;
import java.util.Objects;

final class shapes {
    record Pt(long x, long y) {
    }

    record Seg(Pt a, Pt b, long tag) {
    }

    static final class Counter {
        long hits;
        final String name;

        Counter(long hits, String name) {
            this.hits = hits;
            this.name = name;
        }

        Counter copy() {
            return new Counter(hits, name);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Counter that && Arrays.equals(new Object[] {hits, name}, new Object[] {that.hits, that.name});
        }

        @Override
        public int hashCode() {
            return Objects.hash(hits, name);
        }
    }

    static long measure(Seg s, Counter c, String word) {
        final long dx = s.b.x - s.a.x;
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(dx);
        xs.add(7L);
        final long[] cells = new long[3];
        c.hits = c.hits + dx;
        return (long) xs.size() + c.hits;
    }

    static long main() {
        final Seg seg = new Seg(new Pt(1L, 2L), new Pt(4L, 6L), 9L);
        final Counter c = new Counter(0L, "ctr");
        return measure(seg, c, "hello");
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
