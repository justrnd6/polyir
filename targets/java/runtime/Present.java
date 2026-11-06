// The value an optional holds; an empty one stops the program.
static <T> T present(T value) {
    if (value == null) {
        fail("unwrap of an empty optional");
    }
    return value;
}
