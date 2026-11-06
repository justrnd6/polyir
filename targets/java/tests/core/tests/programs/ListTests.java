import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

final class ListTests {
    static double offText(String got, String want) {
        if (Objects.equals(got, want)) {
            return 0.0;
        }
        return 1.0;
    }

    static double checkFirstAndLast() {
        final ArrayList<Long> xs = new ArrayList<>(List.of(3L, 1L, 4L));
        final ArrayList<String> words = new ArrayList<>(List.of("a", "b"));
        return offText(String.valueOf(first(xs)) + " " + String.valueOf(last(xs)) + " " + first(words) + last(words), "3 4 ab");
    }

    static double checkDrop() {
        final ArrayList<Long> xs = new ArrayList<>(List.of(1L, 2L, 3L));
        final ArrayList<Long> rest = drop(xs, 1L);
        final ArrayList<Long> none = drop(xs, 5L);
        return offText(String.valueOf((long) rest.size()) + " " + String.valueOf(rest.get(0)) + " " + String.valueOf(rest.get(1)) + " " + String.valueOf((long) none.size()), "2 2 3 0");
    }

    static <T> T first(ArrayList<T> xs) {
        return xs.get(0);
    }

    static <T> T last(ArrayList<T> xs) {
        return xs.get((int) ((long) xs.size() - 1L));
    }

    static <T> ArrayList<T> drop(ArrayList<T> xs, long n) {
        final ArrayList<T> out = new ArrayList<>();
        for (long i = n; i < (long) xs.size(); i = i + 1L) {
            out.add(xs.get((int) i));
        }
        return out;
    }
}
