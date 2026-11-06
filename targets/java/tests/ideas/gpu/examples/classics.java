import java.util.Objects;

final class classics {
    static long main() {
        final long device = Metal.metalDevice();
        if (device == 0L) {
            return 10L;
        }
        final String source = Metal.metalReadFile("classics.metal");
        if (Objects.equals(source, "")) {
            return 11L;
        }
        return classics.runAll(device, source);
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
