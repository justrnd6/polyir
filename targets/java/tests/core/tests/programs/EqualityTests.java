import java.util.ArrayList;
import java.util.Arrays;
import java.util.Objects;

final class EqualityTests {
    record Point(long x, long y) {
    }

    record Line(Point a, Point b) {
    }

    record Named(String name, ArrayList<Long> tags) {
    }

    static final class Counter {
        long n;
        final long id;

        Counter(long n, long id) {
            this.n = n;
            this.id = id;
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

    record Digit(long v) {
        public boolean equals(Digit other) {
            return this.v % 10L == other.v % 10L;
        }
    }

    record Wrapped(Either<Long, String> inner) {
    }

    static double checkStructsAreEqualFieldByField() {
        final Point p = new Point(1L, 2L);
        final Point q = new Point(1L, 2L);
        final Point r = new Point(1L, 3L);
        long score = 0L;
        if (Objects.equals(p, q)) {
            score = score + 1L;
        }
        if (!Objects.equals(p, r)) {
            score = score + 2L;
        }
        if (Objects.equals(p, r)) {
            score = score + 100L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkNestedStructsAreEqualRecursively() {
        final Point p = new Point(1L, 2L);
        final Point z = new Point(3L, 4L);
        final Line a = new Line(p, z);
        final Line b = new Line(new Point(1L, 2L), new Point(3L, 4L));
        final Line c = new Line(z, p);
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (!Objects.equals(a, c)) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkAStringAndAListInsideAStructAreComparedByContent() {
        final ArrayList<Long> t1 = new ArrayList<>();
        t1.add(1L);
        final ArrayList<Long> t2 = new ArrayList<>();
        t2.add(1L);
        final Named a = new Named("a" + "b", t1);
        final Named b = new Named("ab", t2);
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        t2.add(2L);
        if (!Objects.equals(a, b)) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkStringsCompareByCharacters() {
        final String x = "match";
        final String y = "ma" + "tch";
        long score = 0L;
        if (Objects.equals(x, y)) {
            score = score + 1L;
        }
        if (!Objects.equals(x, "other")) {
            score = score + 2L;
        }
        if (x == y) {
            score = score + 100L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkAStructWithVarFieldsIsComparedByWhatItHoldsNow() {
        final Counter a = new Counter(1L, 2L);
        final Counter b = new Counter(1L, 2L);
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        b.n = 5L;
        if (!Objects.equals(a, b)) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkIdentityIsTheSameObject() {
        final Counter a = new Counter(1L, 2L);
        final Counter b = new Counter(1L, 2L);
        final Counter alias = a;
        long score = 0L;
        if (a == alias) {
            score = score + 1L;
        }
        if (a != b) {
            score = score + 2L;
        }
        if (a == b) {
            score = score + 100L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkAnImmutableStructComparedByIdentityIsAnObject() {
        final Point p = new Point(1L, 2L);
        final Point q = new Point(1L, 2L);
        final Point same = p;
        long score = 0L;
        if (p == same) {
            score = score + 1L;
        }
        if (p != q) {
            score = score + 2L;
        }
        if (Objects.equals(p, q)) {
            score = score + 4L;
        }
        return Check.offInt(score, 7L);
    }

    static double checkAValStructIsADifferentObjectThatIsEqual() {
        final Counter a = new Counter(1L, 2L);
        final Counter b = a.copy();
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (a != b) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkAnEqualsMethodDecidesWhatEqualMeans() {
        final Digit a = new Digit(13L);
        final Digit b = new Digit(23L);
        final Digit c = new Digit(24L);
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (!Objects.equals(a, c)) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkListsAreEqualElementByElement() {
        final ArrayList<Long> a = new ArrayList<>();
        final ArrayList<Long> b = new ArrayList<>();
        a.add(1L);
        a.add(2L);
        b.add(1L);
        b.add(2L);
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        b.add(3L);
        if (!Objects.equals(a, b)) {
            score = score + 2L;
        }
        if (a == b) {
            score = score + 100L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkListsOfStructsAreEqualDeeply() {
        final ArrayList<Point> a = new ArrayList<>();
        final ArrayList<Point> b = new ArrayList<>();
        a.add(new Point(1L, 2L));
        b.add(new Point(1L, 2L));
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        a.add(new Point(5L, 6L));
        b.add(new Point(5L, 7L));
        if (!Objects.equals(a, b)) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload() {
        final Either<Long, Long> a = Either.<Long, Long>ok(5L);
        final Either<Long, Long> b = Either.<Long, Long>ok(5L);
        final Either<Long, Long> c = Either.<Long, Long>err(5L);
        final Either<Long, Long> d = Either.<Long, Long>ok(6L);
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (!Objects.equals(a, c)) {
            score = score + 2L;
        }
        if (!Objects.equals(a, d)) {
            score = score + 4L;
        }
        return Check.offInt(score, 7L);
    }

    static double checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload() {
        final Variant a = new Variant(1L, 2.5);
        final Variant b = new Variant(1L, 2.5);
        final Variant c = new Variant(0L, (long) (2L));
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (!Objects.equals(a, c)) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkAStructHoldingAnEitherIsComparedThroughIt() {
        final Wrapped a = new Wrapped(Either.<Long, String>ok(1L));
        final Wrapped b = new Wrapped(Either.<Long, String>ok(1L));
        final Wrapped c = new Wrapped(Either.<Long, String>err("no"));
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (!Objects.equals(a, c)) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkNanIsNotEqualToItself() {
        final double nan = 0.0 / 0.0;
        long score = 0L;
        if (nan == nan) {
            score = score + 100L;
        }
        if (nan != nan) {
            score = score + 1L;
        }
        return Check.offInt(score, 1L);
    }

    // An `either`: an ok value or an error, and which one it holds.
    static final class Either<T, E> {
        final boolean isOk;
        private final Object held;

        private Either(boolean isOk, Object held) {
            this.isOk = isOk;
            this.held = held;
        }

        static <T, E> Either<T, E> ok(T value) {
            return new Either<>(true, value);
        }

        static <T, E> Either<T, E> err(E value) {
            return new Either<>(false, value);
        }

        @SuppressWarnings("unchecked")
        T okValue() {
            return (T) held;
        }

        @SuppressWarnings("unchecked")
        E errValue() {
            return (E) held;
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Either<?, ?> that && isOk == that.isOk && Objects.equals(held, that.held);
        }

        @Override
        public int hashCode() {
            return Objects.hash(isOk, held);
        }
    }

    // A variant: the number of its case and the value it holds.
    record Variant(long tag, Object value) {}
}
