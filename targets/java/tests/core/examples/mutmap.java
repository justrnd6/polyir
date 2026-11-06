import java.util.HashMap;

final class mutmap {
    static long collisionAndOverwrite() {
        final HashMap<Long, Long> hm = new HashMap<>();
        hm.put(0L, 100L);
        hm.put(5L, 500L);
        hm.put(1L, 10L);
        hm.put(1L, 20L);
        return hm.getOrDefault(0L, 0L) * 10000L + hm.getOrDefault(5L, 0L) * 10L + hm.getOrDefault(1L, 0L);
    }

    static long missingKeyIsZero() {
        final HashMap<Long, Long> hm = new HashMap<>();
        hm.put(0L, 7L);
        return hm.getOrDefault(42L, 0L);
    }

    static long sizeCheck() {
        final HashMap<Long, Long> hm = new HashMap<>();
        hm.put(0L, 1L);
        hm.put(1L, 2L);
        hm.put(1L, 99L);
        return (long) hm.size() * 100L;
    }

    static long stringKeyedWriteAndReadBack() {
        final HashMap<String, Long> hm = new HashMap<>();
        hm.put("one", 1L);
        hm.put("two", 2L);
        hm.put("one", 11L);
        return hm.getOrDefault("one", 0L) * 100L + hm.getOrDefault("two", 0L);
    }

    static long stringKeyMissingIsZero() {
        final HashMap<String, Long> hm = new HashMap<>();
        hm.put("present", 5L);
        return hm.getOrDefault("absent", 0L);
    }

    static long hashmapPtrValueMissIsSafe() {
        final HashMap<Long, String> hm = new HashMap<>();
        hm.put(1L, "present");
        final String missing = hm.getOrDefault(99L, "");
        return 1L;
    }
}
