import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Objects;

final class Tour {
    record Pt(long x, long y) {
    }

    static final class Counter {
        long n;
        final String name;

        Counter(long n, String name) {
            this.n = n;
            this.name = name;
        }

        Counter copy() {
            return new Counter(n, name);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Counter that && Arrays.equals(new Object[] {n, name}, new Object[] {that.n, that.name});
        }

        @Override
        public int hashCode() {
            return Objects.hash(n, name);
        }
    }

    interface Shape {
        long area();
    }

    record Sq(long side) implements Shape {
        public long area() {
            return this.side * this.side;
        }
    }

    record Rect(long w, long h) implements Shape {
        public long area() {
            return this.w * this.h;
        }
    }

    static long totalArea(ArrayList<Shape> shapes) {
        long sum = 0L;
        for (long i = 0L; i < (long) shapes.size(); i = i + 1L) {
            sum = sum + shapes.get((int) i).area();
        }
        return sum;
    }

    static long risky(long x) {
        if (x < 0L) {
            throw new Thrown(7L);
        }
        return x;
    }

    static long guarded(long x) {
        long out = 0L;
        try {
            out = risky(x);
        } catch (Thrown thrown) {
            final long code = thrown.value;
            out = code * 10L;
        }
        return out;
    }

    static long main() {
        long score = 0L;
        final Pt p = new Pt(3L, 4L);
        final Pt q = new Pt(3L, 4L);
        if (Objects.equals(p, q)) {
            score = score + 1L;
        }
        final Counter c = new Counter(5L, "hits");
        final Counter d = c;
        d.n = d.n + 1L;
        if (c == d) {
            score = score + c.n;
        }
        final ArrayList<Shape> shapes = new ArrayList<>();
        shapes.add(new Sq(3L));
        shapes.add(new Rect(2L, 5L));
        score = score + totalArea(shapes);
        final long[] calls = {0L};
        final Fn0<Long> tick = () -> {
            calls[0] = calls[0] + 1L;
            return calls[0];
        };
        final long first = tick.call();
        final long second = tick.call();
        score = score + first + second + calls[0];
        final HashMap<String, Long> stock = new HashMap<>();
        stock.put("apple", 3L);
        stock.put("pear", 4L);
        score = score + stock.getOrDefault("apple", 0L) * stock.getOrDefault("pear", 0L);
        final Box<Long> b = new Box<>(7L);
        score = score + b.value;
        score = score + guarded(5L) + guarded(0L - 1L);
        return score;
    }

    record Box<T>(T value) {
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }

    // What `throw` throws: the code thrown.
    static final class Thrown extends RuntimeException {
        final long value;

        Thrown(long value) {
            super("thrown " + value);
            this.value = value;
        }
    }

    // The functions a closure can be, by how many arguments they take: `Fn1<A, R>` gives an R, `Act1<A>` gives nothing.
    interface Fn0<R> {
        R call();
    }

    interface Fn1<A, R> {
        R call(A a);
    }

    interface Fn2<A, B, R> {
        R call(A a, B b);
    }

    interface Fn3<A, B, C, R> {
        R call(A a, B b, C c);
    }

    interface Fn4<A, B, C, D, R> {
        R call(A a, B b, C c, D d);
    }

    interface Act0 {
        void call();
    }

    interface Act1<A> {
        void call(A a);
    }

    interface Act2<A, B> {
        void call(A a, B b);
    }

    interface Act3<A, B, C> {
        void call(A a, B b, C c);
    }

    interface Act4<A, B, C, D> {
        void call(A a, B b, C c, D d);
    }
}
