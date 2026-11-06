import java.util.Objects;

final class generics {
    static long boxedI64() {
        final Box<Long> b = new Box<>(42L);
        return b.getValue();
    }

    static long boxedBoth() {
        final Box<Long> bi = new Box<>(100L);
        final Box<Double> bd = new Box<>(2.0);
        return bi.getValue() + (long) (double) bd.getValue();
    }

    static long pairOfIntAndDouble() {
        final Pair_i64_double p = new Pair_i64_double(7L, 8.5);
        return p.sumAsInt();
    }

    static long doubledI64() {
        final Doubler_i64 d = new Doubler_i64(21L);
        return d.doubled();
    }

    static long containerOk() {
        final Container<Long> c = new Container<>(Either.<Long, Long>ok(42L));
        return c.unwrapOrDefault(0L);
    }

    static Either<Pair_i64_i64, Long> makePairResult(long ok) {
        if (ok == 0L) {
            return Either.<Pair_i64_i64, Long>err(-1L);
        }
        return Either.<Pair_i64_i64, Long>ok(new Pair_i64_i64(11L, 22L));
    }

    static long pairResultOrErr(long ok) {
        final Either<Pair_i64_i64, Long> r = makePairResult(ok);
        if (!r.isOk) {
            return r.errValue();
        }
        final Pair_i64_i64 p = r.okValue();
        return p.a * 100L + p.b;
    }

    record Box<T>(T value) {
        public T getValue() {
            return this.value;
        }
    }

    record Pair_i64_double(long a, double b) {
        public long sumAsInt() {
            return (long) this.a + (long) (double) this.b;
        }
    }

    record Doubler_i64(long value) {
        public long doubled() {
            return this.value + this.value;
        }
    }

    record Container<T>(Either<T, Long> value) {
        public T unwrapOrDefault(T def) {
            if (!this.value.isOk) {
                return def;
            }
            return this.value.okValue();
        }
    }

    record Pair_i64_i64(long a, long b) {
        public long sumAsInt() {
            return (long) this.a + (long) this.b;
        }
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
}
