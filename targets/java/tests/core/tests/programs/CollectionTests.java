import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;

final class CollectionTests {
    record Point(long x, long y) {
    }

    record Named(String name, Point at) {
    }

    static double checkAnArrayGrowsByMakingANewOne() {
        final ArrayList<Long> a0 = new ArrayList<>();
        final ArrayList<Long> a1 = withItem(a0, 10L);
        final ArrayList<Long> a2 = withItem(a1, 20L);
        return Check.offInt((long) a0.size(), 0L) + Check.offInt((long) a1.size(), 1L) + Check.offInt((long) a2.size(), 2L) + Check.offInt(a2.get(0), 10L) + Check.offInt(a2.get(1), 20L);
    }

    static double checkThawAndFreezeCopy() {
        final ArrayList<Long> a = new ArrayList<>(List.of(1L, 2L));
        final ArrayList<Long> m = new ArrayList<>(a);
        m.add(3L);
        final ArrayList<Long> f = new ArrayList<>(m);
        m.add(4L);
        return Check.offInt((long) a.size(), 2L) + Check.offInt((long) m.size(), 4L) + Check.offInt((long) f.size(), 3L) + Check.offInt(f.get(2), 3L);
    }

    static double checkAMapGrowsByMakingANewOne() {
        final HashMap<Long, Long> m0 = new HashMap<>();
        final HashMap<Long, Long> m1 = withEntry(m0, 1L, 100L);
        HashMap<Long, Long> m = m1;
        for (long i = 2L; i < 30L; i = i + 1L) {
            m = withEntry(m, i, i * 100L);
        }
        return Check.offInt((long) m0.size(), 0L) + Check.offInt((long) m1.size(), 1L) + Check.offInt((long) m.size(), 29L) + Check.offInt(m.getOrDefault(1L, 0L), 100L) + Check.offInt(m.getOrDefault(29L, 0L), 2900L) + Check.offInt(m1.getOrDefault(2L, 0L), 0L);
    }

    static double checkWithReplacesAnExistingKey() {
        final HashMap<Long, Long> m = withEntry(withEntry(new HashMap<>(), 1L, 10L), 1L, 11L);
        return Check.offInt((long) m.size(), 1L) + Check.offInt(m.getOrDefault(1L, 0L), 11L);
    }

    static double checkHasTellsPresentFromAbsent() {
        final HashMap<String, Long> m = withEntry(new HashMap<>(), "a", 0L);
        long score = 0L;
        if (m.containsKey("a")) {
            score = score + 1L;
        }
        if (m.containsKey("b") == false) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkASetKeepsEachMemberOnce() {
        final HashSet<Long> s = withItem(withItem(withItem(new HashSet<>(), 7L), 8L), 7L);
        long score = 0L;
        if (s.contains(7L)) {
            score = score + 1L;
        }
        if (s.contains(9L) == false) {
            score = score + 2L;
        }
        return Check.offInt((long) s.size(), 2L) + Check.offInt(score, 3L);
    }

    static double checkAMutsetChangesInPlace() {
        final HashSet<String> s = new HashSet<>();
        s.add("x");
        s.add("y");
        s.add("x");
        long score = 0L;
        if (s.contains("y")) {
            score = score + 1L;
        }
        if (s.contains("z") == false) {
            score = score + 2L;
        }
        return Check.offInt((long) s.size(), 2L) + Check.offInt(score, 3L);
    }

    static double checkAMutmapChangesInPlace() {
        final HashMap<Long, Long> m = new HashMap<>();
        m.put(1L, 10L);
        m.put(2L, 20L);
        m.put(1L, 11L);
        return Check.offInt((long) m.size(), 2L) + Check.offInt(m.getOrDefault(1L, 0L), 11L) + Check.offInt(m.getOrDefault(2L, 0L), 20L);
    }

    static double checkAStructCanBeAKey() {
        final HashMap<Point, Long> seen = new HashMap<>();
        seen.put(new Point(1L, 2L), 10L);
        seen.put(new Point(3L, 4L), 20L);
        seen.put(new Point(1L, 2L), 11L);
        long score = 0L;
        if (seen.containsKey(new Point(3L, 4L))) {
            score = score + 1L;
        }
        if (seen.containsKey(new Point(9L, 9L)) == false) {
            score = score + 2L;
        }
        return Check.offInt((long) seen.size(), 2L) + Check.offInt(seen.getOrDefault(new Point(1L, 2L), 0L), 11L) + Check.offInt(score, 3L);
    }

    static double checkAStructWithAStringIsAKeyByContent() {
        final HashSet<Named> names = withItem(withItem(withItem(new HashSet<>(), new Named("a" + "b", new Point(1L, 1L))), new Named("ab", new Point(1L, 1L))), new Named("c", new Point(2L, 2L)));
        long score = 0L;
        if (names.contains(new Named("c", new Point(2L, 2L)))) {
            score = score + 1L;
        }
        return Check.offInt((long) names.size(), 2L) + Check.offInt(score, 1L);
    }

    static double checkAnImmutableArrayIsAKey() {
        final HashMap<ArrayList<Long>, String> lists = withEntry(withEntry(new HashMap<>(), new ArrayList<>(List.of(1L, 2L)), "one-two"), new ArrayList<>(List.of(3L)), "three");
        long score = 0L;
        if (Objects.equals(lists.getOrDefault(new ArrayList<>(List.of(1L, 2L)), ""), "one-two")) {
            score = score + 1L;
        }
        if (Objects.equals(lists.getOrDefault(new ArrayList<>(List.of(3L)), ""), "three")) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkMapsAreEqualWhateverTheOrderTheyWereBuiltIn() {
        final HashMap<Long, String> a = withEntry(withEntry(new HashMap<>(), 1L, "a"), 2L, "b");
        final HashMap<Long, String> b = withEntry(withEntry(new HashMap<>(), 2L, "b"), 1L, "a");
        final HashMap<Long, String> c = withEntry(withEntry(new HashMap<>(), 2L, "b"), 1L, "z");
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (!Objects.equals(a, c)) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkSetsAreEqualWhateverTheOrderTheyWereBuiltIn() {
        final HashSet<String> a = withItem(withItem(new HashSet<>(), "x"), "y");
        final HashSet<String> b = withItem(withItem(new HashSet<>(), "y"), "x");
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (!Objects.equals(a, withItem(new HashSet<>(), "x"))) {
            score = score + 2L;
        }
        return Check.offInt(score, 3L);
    }

    static double checkMutableAndImmutableListsAreEqualByContent() {
        final ArrayList<Long> a = new ArrayList<>(List.of(1L, 2L));
        final ArrayList<Long> b = new ArrayList<>(List.of(1L, 2L));
        long score = 0L;
        if (Objects.equals(a, b)) {
            score = score + 1L;
        }
        if (a == b) {
            score = score + 100L;
        }
        return Check.offInt(score, 1L);
    }

    static double checkRepeatedWithInALoopIsNotQuadraticInMemory() {
        long total = 0L;
        for (long r = 0L; r < 200L; r = r + 1L) {
            ArrayList<Long> a = new ArrayList<>();
            HashMap<Long, Long> m = new HashMap<>();
            HashSet<Long> s = new HashSet<>();
            for (long i = 0L; i < 20L; i = i + 1L) {
                a = withItem(a, i);
                m = withEntry(m, i, i * 2L);
                s = withItem(s, i % 7L);
            }
            total = total + (long) a.size() + m.getOrDefault(19L, 0L) + (long) s.size();
        }
        return Check.offInt(total, 200L * 65L);
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
