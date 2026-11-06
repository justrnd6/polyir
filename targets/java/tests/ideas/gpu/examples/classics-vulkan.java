final class classics_vulkan {
    static long main() {
        final long gpu = Vulkan.vulkanStart();
        if (gpu == 0L) {
            return 10L;
        }
        return classics.runAll(gpu, "");
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
