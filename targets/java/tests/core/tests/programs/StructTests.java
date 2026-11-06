import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Objects;

final class StructTests {
    static final class Counter {
        long n;
        final long id;

        Counter(long n, long id) {
            this.n = n;
            this.id = id;
        }

        public long bump() {
            this.n = this.n + 1L;
            return this.n;
        }

        Counter copy() {
            return new Counter(n, id);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Counter that && Arrays.equals(new Object[] {n, id}, new Object[] {that.n, that.id});
        }

        @Override
        public int hashCode() {
            return Objects.hash(n, id);
        }
    }

    record Pt(long x, long y) {
    }

    static final class Holder {
        ArrayList<Long> items;
        Counter inner;

        Holder(ArrayList<Long> items, Counter inner) {
            this.items = items;
            this.inner = inner;
        }

        Holder copy() {
            return new Holder(items, inner);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Holder that && Arrays.equals(new Object[] {items, inner}, new Object[] {that.items, that.inner});
        }

        @Override
        public int hashCode() {
            return Objects.hash(items, inner);
        }
    }

    interface Reading {
        long read();
    }

    static final class Gauge implements Reading {
        long level;

        Gauge(long level) {
            this.level = level;
        }

        public long read() {
            return this.level;
        }

        Gauge copy() {
            return new Gauge(level);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Gauge that && Arrays.equals(new Object[] {level}, new Object[] {that.level});
        }

        @Override
        public int hashCode() {
            return Objects.hash(level);
        }
    }

    static long addTen(Counter c) {
        c.n = c.n + 10L;
        return 0L;
    }

    static long addTenOnACopy(Counter c) {
        final Counter c2 = c.copy();
        c2.n = c2.n + 10L;
        return c2.n;
    }

    static double checkTwoNamesForOneObjectShareIt() {
        final Counter a = new Counter(1L, 7L);
        final Counter b = a;
        b.n = 5L;
        return Check.offInt(a.n, 5L) + Check.offInt(a.id, 7L);
    }

    static double checkACalleeChangesTheCallersObject() {
        final Counter a = new Counter(1L, 0L);
        addTen(a);
        return Check.offInt(a.n, 11L);
    }

    static double checkAValParameterIsTheCalleesOwnCopy() {
        final Counter a = new Counter(1L, 0L);
        final long seen = addTenOnACopy(a);
        return Check.offInt(seen, 11L) + Check.offInt(a.n, 1L);
    }

    static double checkValInFrontOfAnExpressionCopies() {
        final Counter a = new Counter(1L, 3L);
        final Counter b = a.copy();
        b.n = 50L;
        return Check.offInt(a.n, 1L) + Check.offInt(b.n, 50L) + Check.offInt(b.id, 3L);
    }

    static double checkCopyMethodAndValAgree() {
        final Counter a = new Counter(4L, 0L);
        final Counter b = a.copy();
        a.n = 9L;
        return Check.offInt(b.n, 4L);
    }

    static double checkAMethodChangesThis() {
        final Counter a = new Counter(0L, 0L);
        a.bump();
        a.bump();
        return Check.offInt(a.n, 2L);
    }

    static double checkAnImmutableStructIsAValue() {
        final Pt p = new Pt(1L, 2L);
        final Pt q = p;
        final Pt r = p;
        return Check.offInt(q.x + r.y, 3L);
    }

    static double checkAFieldHoldingAnotherReferenceStruct() {
        final Holder h = new Holder(new ArrayList<>(), new Counter(1L, 0L));
        final Holder alias = h;
        alias.inner.n = 8L;
        alias.items.add(5L);
        return Check.offInt(h.inner.n, 8L) + Check.offInt((long) h.items.size(), 1L);
    }

    static double checkAFieldCanBeReplacedWithAnotherObject() {
        final Holder h = new Holder(new ArrayList<>(), new Counter(1L, 0L));
        final Counter keep = h.inner;
        h.inner = new Counter(2L, 0L);
        return Check.offInt(keep.n, 1L) + Check.offInt(h.inner.n, 2L);
    }

    static double checkAGenericStructWithAVarField() {
        final Cell<Long> c = new Cell<>(1L);
        final Cell<Long> d = c;
        d.v = 9L;
        return Check.offInt(c.v, 9L);
    }

    static double checkAnInterfaceValueSeesLaterChanges() {
        final Gauge g = new Gauge(1L);
        final Reading r = g;
        g.level = 42L;
        return Check.offInt(r.read(), 42L);
    }

    static double checkAStructInADarrayIsTheSameObject() {
        final ArrayList<Counter> all = new ArrayList<>();
        final Counter a = new Counter(1L, 0L);
        all.add(a);
        a.n = 6L;
        return Check.offInt(all.get(0).n, 6L);
    }

    static double checkAStructInAClosureIsShared() {
        final Counter a = new Counter(1L, 0L);
        final Fn0<Long> f = () -> {
            a.n = a.n + 1L;
            return a.n;
        };
        f.call();
        return Check.offInt(a.n, 2L);
    }

    static Counter makeCounter(long start) {
        final Counter c = new Counter(start, 1L);
        c.n = c.n + 1L;
        return c;
    }

    static long lend(Counter c) {
        return c.n * 2L;
    }

    static long keepIn(ArrayList<Counter> store, Counter c) {
        store.add(c);
        return 0L;
    }

    static double checkAReturnedObjectSurvives() {
        final Counter a = makeCounter(5L);
        final Counter b = makeCounter(10L);
        return Check.offInt(a.n * 100L + b.n, 611L);
    }

    static double checkAnObjectLentToACalleeIsUsableAfterwards() {
        long total = 0L;
        for (long i = 0L; i < 100L; i = i + 1L) {
            final Counter c = new Counter(i, 0L);
            total = total + lend(c) + c.n;
            c.n = 0L;
        }
        return Check.offInt(total, 3L * 4950L);
    }

    static double checkAnObjectAnotherCalleeKeepsSurvives() {
        final ArrayList<Counter> store = new ArrayList<>();
        for (long i = 0L; i < 4L; i = i + 1L) {
            final Counter c = new Counter(i * 10L, 0L);
            keepIn(store, c);
        }
        return Check.offInt(store.get(0).n + store.get(1).n + store.get(2).n + store.get(3).n, 60L);
    }

    static double checkAValParameterCopyIsReleasedAndTheOriginalIsNot() {
        final Counter a = new Counter(1L, 0L);
        long total = 0L;
        for (long i = 0L; i < 50L; i = i + 1L) {
            total = total + addTenOnACopy(a);
        }
        return Check.offInt(total, 50L * 11L) + Check.offInt(a.n, 1L);
    }

    static long earlyReturnWithObjects(long n) {
        final Counter c = new Counter(n, 0L);
        if (n > 5L) {
            return c.n * 2L;
        }
        c.bump();
        return c.n;
    }

    static double checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue() {
        return Check.offInt(earlyReturnWithObjects(9L) * 100L + earlyReturnWithObjects(2L), 1800L + 3L);
    }

    static double checkLetVarAndValOnReferenceStructs() {
        final Counter a = new Counter(1L, 0L);
        final Counter alias = a;
        final Counter copy = a.copy();
        Counter moving = a;
        a.n = 10L;
        moving = new Counter(100L, 0L);
        moving.n = moving.n + 1L;
        return Check.offInt(alias.n, 10L) + Check.offInt(copy.n, 1L) + Check.offInt(moving.n, 101L) + Check.offInt(a.n, 10L);
    }

    static double checkValOnAContainerIsAnIndependentCopy() {
        final ArrayList<Long> d = new ArrayList<>();
        d.add(4L);
        final ArrayList<Long> e = new ArrayList<>(d);
        d.add(5L);
        return Check.offInt((long) e.size(), 1L) + Check.offInt((long) d.size(), 2L);
    }

    record Dot(long x, long y) {
    }

    record Span(long lo, long hi) {
        public long width() {
            return this.hi - this.lo;
        }
    }

    static final class Meter {
        long reading;
        final long id;

        Meter(long reading, long id) {
            this.reading = reading;
            this.id = id;
        }

        public long bump() {
            this.reading = this.reading + 1L;
            return this.reading;
        }

        Meter copy() {
            return new Meter(reading, id);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Meter that && Arrays.equals(new Object[] {reading, id}, new Object[] {that.reading, that.id});
        }

        @Override
        public int hashCode() {
            return Objects.hash(reading, id);
        }
    }

    record Plate(long code) implements Reading {
        public long read() {
            return this.code;
        }
    }

    static double checkShorthandStructsWork() {
        final Dot d = new Dot(3L, 4L);
        final Span s = new Span(2L, 9L);
        final Wrapper<Long> w = new Wrapper<>(5L);
        final Reading r = new Plate(7L);
        return Check.offInt(d.x + d.y, 7L) + Check.offInt(s.width(), 7L) + Check.offInt(w.value, 5L) + Check.offInt(r.read(), 7L);
    }

    static double checkAShorthandStructWithAVarFieldIsAReference() {
        final Meter m = new Meter(1L, 2L);
        final Meter alias = m;
        alias.bump();
        return Check.offInt(m.reading, 2L) + Check.offInt(m.id, 2L);
    }

    static double checkAHashmapOfObjectsIsUsableThenReleased() {
        long total = 0L;
        for (long i = 0L; i < 100L; i = i + 1L) {
            final HashMap<Long, Counter> m = new HashMap<>();
            m.put(1L, new Counter(i, 1L));
            m.put(2L, new Counter(5L, 2L));
            m.put(2L, new Counter(6L, 2L));
            m.getOrDefault(1L, null).bump();
            total = total + m.getOrDefault(1L, null).n - i + m.getOrDefault(2L, null).n + (long) m.size();
        }
        return Check.offInt(total, 100L * (1L + 6L + 2L));
    }

    static double checkASparseOfStringsIsUsableThenReleased() {
        long total = 0L;
        for (long i = 0L; i < 100L; i = i + 1L) {
            final Sparse<String> s = new Sparse<>("");
            s.set("a" + "b", 0L, 0L);
            s.set("c" + "d", 2L, 3L);
            if (Objects.equals(s.get(2L, 3L), "cd")) {
                total = total + (long) s.size();
            }
        }
        return Check.offInt(total, 200L);
    }

    static final class Cell<T> {
        T v;

        Cell(T v) {
            this.v = v;
        }

        Cell copy() {
            return new Cell(v);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Cell that && Arrays.equals(new Object[] {v}, new Object[] {that.v});
        }

        @Override
        public int hashCode() {
            return Objects.hash(v);
        }
    }

    record Wrapper<T>(T value) {
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

    // A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
    // written reads as the element's zero. `set(value, row, col)` writes one.
    static final class Sparse<T> {
        private final ArrayList<Long> rows = new ArrayList<>();
        private final ArrayList<Long> cols = new ArrayList<>();
        private final ArrayList<T> values = new ArrayList<>();
        private final T zero;

        Sparse(T zero) {
            this.zero = zero;
        }

        long size() {
            return values.size();
        }

        T get(long row, long col) {
            T found = zero;
            for (int i = 0; i < values.size(); i++) {
                if (rows.get(i) == row && cols.get(i) == col) {
                    found = values.get(i);
                }
            }
            return found;
        }

        void set(T value, long row, long col) {
            rows.add(row);
            cols.add(col);
            values.add(value);
        }

        Sparse<T> copy() {
            final Sparse<T> out = new Sparse<>(zero);
            out.rows.addAll(rows);
            out.cols.addAll(cols);
            out.values.addAll(values);
            return out;
        }
    }
}
