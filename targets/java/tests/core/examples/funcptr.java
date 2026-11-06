final class funcptr {
    static long doubleIt(long x) {
        return x * 2L;
    }

    static long applyTwice(Fn1<Long, Long> f, long x) {
        final long once = f.call(x);
        return f.call(once);
    }

    static long applyDoubleTwice(long x) {
        return applyTwice(funcptr::doubleIt, x);
    }

    static long directDoubleCheck(long x) {
        return doubleIt(x);
    }

    static long addOne(long x) {
        return x + 1L;
    }

    static long pickAndApply(long x, long useAddOne) {
        Fn1<Long, Long> fn = funcptr::doubleIt;
        if (useAddOne != 0L) {
            fn = funcptr::addOne;
        }
        return fn.call(x);
    }

    static long applyNTimes(Fn1<Long, Long> f, long x, long n) {
        long acc = x;
        for (long i = 0L; i < n; i = i + 1L) {
            acc = f.call(acc);
        }
        return acc;
    }

    static long doubleFourTimes(long x) {
        return applyNTimes(funcptr::doubleIt, x, 4L);
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
