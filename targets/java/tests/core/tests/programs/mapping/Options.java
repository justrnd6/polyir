import java.util.ArrayList;
import java.util.Objects;

final class Options {
    record Pt(long x, long y) {
    }

    record Contact(String name, String nickname) {
    }

    static Long find(ArrayList<Long> xs, long want) {
        for (long i = 0L; i < (long) xs.size(); i = i + 1L) {
            if (xs.get((int) i) == want) {
                return i;
            }
        }
        return null;
    }

    static Pt firstPoint(long ok) {
        if (ok == 1L) {
            return new Pt(7L, 8L);
        }
        return null;
    }

    static String shown(Contact c) {
        if (c.nickname != null) {
            return present(c.nickname);
        }
        return c.name;
    }

    static long main() {
        final ArrayList<Long> xs = new ArrayList<>();
        xs.add(10L);
        xs.add(20L);
        long score = 0L;
        final Long hit = find(xs, 20L);
        if (hit != null) {
            score = score + present(hit);
        }
        if (find(xs, 99L) == null) {
            score = score + 10L;
        }
        final Pt p = firstPoint(1L);
        if (p != null) {
            score = score + present(p).x;
        }
        if (firstPoint(0L) == null) {
            score = score + 100L;
        }
        final Contact named = new Contact("ann", "annie");
        final Contact plain = new Contact("bo", null);
        if (Objects.equals(shown(named), "annie")) {
            score = score + 1000L;
        }
        if (Objects.equals(shown(plain), "bo")) {
            score = score + 2000L;
        }
        return score;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }

    // Stops the program with a message, as polyir's `fail` does.
    static <T> T fail(String message) {
        System.err.println("trap: " + message);
        System.exit(1);
        return null;
    }

    // The value an optional holds; an empty one stops the program.
    static <T> T present(T value) {
        if (value == null) {
            fail("unwrap of an empty optional");
        }
        return value;
    }
}
