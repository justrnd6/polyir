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
