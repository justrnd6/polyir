final class host_types {
    record Job(long id) implements java.lang.Runnable {
        public long run() {
            return this.id;
        }
    }

    static long main() {
        return 0L;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
