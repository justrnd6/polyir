import java.util.ArrayList;

final class Lists {
    static <T> ArrayList<T> drop(ArrayList<T> xs, long n) {
        final ArrayList<T> out = new ArrayList<>();
        for (long i = n; i < (long) xs.size(); i = i + 1L) {
            out.add(xs.get((int) i));
        }
        return out;
    }

    static <T> T first(ArrayList<T> xs) {
        return xs.get(0);
    }

    static <T> T last(ArrayList<T> xs) {
        return xs.get((int) ((long) xs.size() - 1L));
    }
}
