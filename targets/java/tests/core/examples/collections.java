import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Objects;

final class collections {
    record Point(long x, long y) {
    }

    static long grow() {
        final ArrayList<Long> none = new ArrayList<>();
        final ArrayList<Long> one = withItem(none, 10L);
        final ArrayList<Long> two = withItem(one, 20L);
        return (long) none.size() * 1000L + (long) one.size() * 100L + (long) two.size() * 10L + two.get(1) / 10L;
    }

    static ArrayList<Long> build() {
        final ArrayList<Long> xs = new ArrayList<>();
        for (long i = 0L; i < 5L; i = i + 1L) {
            xs.add(i * i);
        }
        return new ArrayList<>(xs);
    }

    static long lookup() {
        final HashMap<String, Long> ages = withEntry(withEntry(new HashMap<>(), "ann", 31L), "bo", 27L);
        final HashSet<Long> seen = withItem(withItem(withItem(new HashSet<>(), 3L), 5L), 3L);
        long score = ages.getOrDefault("ann", 0L) + ages.getOrDefault("bo", 0L) + (long) seen.size();
        if (ages.containsKey("cy") == false) {
            score = score + 100L;
        }
        if (seen.contains(5L)) {
            score = score + 1000L;
        }
        return score;
    }

    static long visited() {
        final HashSet<Point> seen = new HashSet<>();
        seen.add(new Point(1L, 2L));
        seen.add(new Point(3L, 4L));
        seen.add(new Point(1L, 2L));
        long score = (long) seen.size();
        if (seen.contains(new Point(3L, 4L))) {
            score = score + 10L;
        }
        return score;
    }

    static long sameThings() {
        final HashMap<Long, String> a = withEntry(withEntry(new HashMap<>(), 1L, "a"), 2L, "b");
        final HashMap<Long, String> b = withEntry(withEntry(new HashMap<>(), 2L, "b"), 1L, "a");
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (a == b) {
            score = score + 100L;
        }
        return score;
    }

    static long main() {
        return grow() + build().get(4) * 10000L + lookup() + visited() + sameThings();
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }

    // A new list, set or map with one more item.
    static <T> ArrayList<T> withItem(ArrayList<T> items, T item) {
        final ArrayList<T> out = new ArrayList<>(items);
        out.add(item);
        return out;
    }

    static <T> HashSet<T> withItem(HashSet<T> items, T item) {
        final HashSet<T> out = new HashSet<>(items);
        out.add(item);
        return out;
    }

    static <K, V> HashMap<K, V> withEntry(HashMap<K, V> table, K key, V value) {
        final HashMap<K, V> out = new HashMap<>(table);
        out.put(key, value);
        return out;
    }
}
