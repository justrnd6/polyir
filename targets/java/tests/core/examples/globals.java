final class globals {
    static long counter = 0L;

    static long bump() {
        counter = counter + 1L;
        return counter;
    }
}
