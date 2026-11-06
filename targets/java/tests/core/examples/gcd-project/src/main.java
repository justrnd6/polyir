final class mainFile {
    static long main() {
        return euclid.gcd(48L, 18L) * 10L + euclid.lcm(4L, 6L);
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
