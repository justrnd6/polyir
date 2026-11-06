import java.util.Objects;

final class variant {
    static Either<Long, Long> safeDivide(long a, long b) {
        if (b == 0L) {
            return Either.<Long, Long>err(99L);
        }
        return Either.<Long, Long>ok(a / b);
    }

    static long divideOrDefault(long a, long b) {
        final Either<Long, Long> r = safeDivide(a, b);
        if (!r.isOk) {
            return r.errValue();
        }
        return r.okValue();
    }

    static long eitherCoercion() {
        final Either<Double, Long> r = Either.<Double, Long>ok(3.0);
        return (long) (double) r.okValue();
    }

    static long variantSum(long which) {
        Variant v = new Variant(0L, (long) (0L));
        if (which == 0L) {
            v = new Variant(0L, (long) (10L));
        }
        if (which == 1L) {
            v = new Variant(1L, 2.5);
        }
        if (which == 2L) {
            v = new Variant(2L, (long) (30L));
        }
        if (v.tag() == 0L) {
            return (Long) v.value();
        }
        if (v.tag() == 1L) {
            return (long) (double) (Double) v.value() * 100L;
        }
        return (Long) v.value() * 1000L;
    }

    static long castRoundTrip() {
        final double d = 9.0;
        final long asI64 = (long) (double) d;
        final double backToD = (double) asI64;
        return (long) (double) backToD + asI64;
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
