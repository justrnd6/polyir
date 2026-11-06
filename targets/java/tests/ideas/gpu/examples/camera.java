import java.util.Objects;

final class camera {
    static long main() {
        final long device = Metal.metalDevice();
        if (device == 0L) {
            return 10L;
        }
        final String source = Metal.metalReadFile("camera.metal");
        if (Objects.equals(source, "")) {
            return 11L;
        }
        return camera.runAll(device, source);
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
