import java.util.ArrayList;
import java.util.Arrays;
import java.util.Objects;

final class Methods {
    static final class Acc {
        long total;
        String tag;
        ArrayList<Long> items;

        Acc(long total, String tag, ArrayList<Long> items) {
            this.total = total;
            this.tag = tag;
            this.items = items;
        }

        public long add(long x) {
            this.total = this.total + x;
            this.items.add(x);
            return this.total;
        }

        public Fn0<Long> later() {
            return () -> this.total * 2L;
        }

        public String describe() {
            return this.tag + ":" + this.tag;
        }

        public Acc self() {
            return this;
        }

        Acc copy() {
            return new Acc(total, tag, items);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Acc that && Arrays.equals(new Object[] {total, tag, items}, new Object[] {that.total, that.tag, that.items});
        }

        @Override
        public int hashCode() {
            return Objects.hash(total, tag, items);
        }
    }

    interface Animal {
        long legs(long p0);
        String name();
    }

    record Dog(long age) implements Animal {
        public long legs(long n) {
            return 4L * n + this.age;
        }

        public String name() {
            return "dog";
        }
    }

    record Bird(long age) implements Animal {
        public long legs(long n) {
            return 2L * n;
        }

        public String name() {
            return "bird";
        }
    }

    static long risky(long x) {
        if (x < 0L) {
            throw new Thrown(7L);
        }
        return x;
    }

    static long tryReturn(long x) {
        try {
            return risky(x) + 1L;
        } catch (Thrown thrown) {
            final long code = thrown.value;
            return code * 100L;
        }
        return 0L;
    }

    static long nested(long x) {
        long r = 0L;
        try {
            try {
                r = risky(x);
            } catch (Thrown thrown) {
                final long a = thrown.value;
                r = a + 1L;
                throw new Thrown(r);
            }
        } catch (Thrown thrown) {
            final long b = thrown.value;
            r = b * 10L;
        }
        return r;
    }

    static Fn1<Long, Long> makeAdder(long n) {
        return (x) -> x + n;
    }

    static long twice(Fn1<Long, Long> f, long x) {
        return f.call(f.call(x));
    }

    static long counterPair() {
        final long[] c = {0L};
        final Fn0<Long> inc = () -> {
            c[0] = c[0] + 1L;
            return c[0];
        };
        final Fn0<Long> get = () -> c[0];
        inc.call();
        inc.call();
        return get.call() * 10L + c[0];
    }

    static long main() {
        long score = 0L;
        final Acc a = new Acc(1L, "ab", new ArrayList<>());
        score = score + a.add(5L) + a.add(6L);
        final Fn0<Long> f = a.later();
        score = score + f.call();
        score = score + (long) a.items.size();
        if (Objects.equals(a.describe(), "ab:ab")) {
            score = score + 1000L;
        }
        final Acc s = a.self();
        if (s == a) {
            score = score + 2000L;
        }
        a.tag = "zz";
        if (Objects.equals(s.tag, "zz")) {
            score = score + 3000L;
        }
        final ArrayList<Animal> zoo = new ArrayList<>();
        zoo.add(new Dog(3L));
        zoo.add(new Bird(1L));
        for (long i = 0L; i < (long) zoo.size(); i = i + 1L) {
            score = score + zoo.get((int) i).legs(2L);
            if (Objects.equals(zoo.get((int) i).name(), "bird")) {
                score = score + 7L;
            }
        }
        score = score + tryReturn(3L) + tryReturn(0L - 1L);
        score = score + nested(4L) + nested(0L - 2L);
        final Fn1<Long, Long> add3 = makeAdder(3L);
        score = score + twice(add3, 10L);
        score = score + counterPair();
        return score % 251L;
    }

    public static void main(String[] args) {
        System.exit((int) main());
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

    // What `throw` throws: the code thrown.
    static final class Thrown extends RuntimeException {
        final long value;

        Thrown(long value) {
            super("thrown " + value);
            this.value = value;
        }
    }
}
