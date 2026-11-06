final class pixels_vulkan {
    static long main() {
        final long gpu = Vulkan.vulkanStart();
        if (gpu == 0L) {
            return 10L;
        }
        pixels.printTable(pixels.facePicture());
        long code = pixels.scanDemo(gpu, "");
        if (code != 0L) {
            return code;
        }
        code = pixels.channelsDemo(gpu, "");
        if (code != 0L) {
            return code;
        }
        code = pixels.bitsDemo(gpu, "");
        if (code != 0L) {
            return code;
        }
        code = pixels.resizeDemo(gpu, "");
        if (code != 0L) {
            return code;
        }
        return pixels.enlargeDemo(gpu, "");
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
