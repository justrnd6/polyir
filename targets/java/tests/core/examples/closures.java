final class closures {
    static long counterTwice() {
        final long[] count = {0L};
        final Fn0<Long> inc = () -> {
            count[0] = count[0] + 1L;
            return count[0];
        };
        final long a = inc.call();
        final long b = inc.call();
        return a * 100L + b;
    }

    static Fn1<Long, Long> makeMultiplier(long factor) {
        final Fn1<Long, Long> mul = (x) -> x * factor;
        return mul;
    }

    static long tripleOf(long x) {
        final Fn1<Long, Long> triple = makeMultiplier(3L);
        return triple.call(x);
    }

    static long sharedCounter() {
        final long[] shared = {0L};
        final Fn0<Long> bumpBy1 = () -> {
            shared[0] = shared[0] + 1L;
            return shared[0];
        };
        final Fn0<Long> bumpBy10 = () -> {
            shared[0] = shared[0] + 10L;
            return shared[0];
        };
        final long a = bumpBy1.call();
        final long b = bumpBy10.call();
        final long c = bumpBy1.call();
        return a * 10000L + b * 100L + c;
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
