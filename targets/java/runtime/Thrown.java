// What `throw` throws: the code thrown.
static final class Thrown extends RuntimeException {
    final long value;

    Thrown(long value) {
        super("thrown " + value);
        this.value = value;
    }
}
