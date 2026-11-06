import java.util.Objects;

final class pixels {
    static long main() {
        final long device = Metal.metalDevice();
        if (device == 0L) {
            return 10L;
        }
        final String source = Metal.metalReadFile("pixels.metal");
        if (Objects.equals(source, "")) {
            return 11L;
        }
        pixels.printTable(pixels.facePicture());
        long code = pixels.scanDemo(device, source);
        if (code != 0L) {
            return code;
        }
        code = pixels.channelsDemo(device, source);
        if (code != 0L) {
            return code;
        }
        code = pixels.bitsDemo(device, source);
        if (code != 0L) {
            return code;
        }
        code = pixels.resizeDemo(device, source);
        if (code != 0L) {
            return code;
        }
        return pixels.enlargeDemo(device, source);
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
