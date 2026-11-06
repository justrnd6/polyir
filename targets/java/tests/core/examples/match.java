import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

final class match {
    static boolean startsWith(String s, String prefix) {
        if (textLength(prefix) > textLength(s)) {
            return false;
        }
        return Objects.equals(textSlice(s, 0L, textLength(prefix)), prefix);
    }

    static boolean endsWith(String s, String suffix) {
        if (textLength(suffix) > textLength(s)) {
            return false;
        }
        return Objects.equals(textSlice(s, textLength(s) - textLength(suffix), textLength(s)), suffix);
    }

    static String sizeOf(long n) {
        String matchValue_10_10 = "";
        if (n == 0L) {
            matchValue_10_10 = "none";
        } else if (n == 1L || n == 2L || n == 3L) {
            matchValue_10_10 = "a few";
        } else {
            matchValue_10_10 = "many";
        }
        return matchValue_10_10;
    }

    static String gradeOf(long score) {
        String matchValue_19_10 = "";
        if (score >= 90L) {
            matchValue_19_10 = "A";
        } else if (score >= 80L) {
            matchValue_19_10 = "B";
        } else {
            matchValue_19_10 = "C";
        }
        return matchValue_19_10;
    }

    static String kindOf(String path) {
        String matchValue_28_10 = "";
        if (Objects.equals(path, "Ninafile") || Objects.equals(path, "Polyfile")) {
            matchValue_28_10 = "build";
        } else if (endsWith(path, ".poly")) {
            matchValue_28_10 = "polyir";
        } else if (startsWith(path, "README")) {
            matchValue_28_10 = "docs";
        } else {
            matchValue_28_10 = "other";
        }
        return matchValue_28_10;
    }

    static String shown(Long found) {
        String matchValue_38_10 = "";
        if (found != null && present(found) > 100L) {
            final long n = present(found);
            matchValue_38_10 = "big " + String.valueOf(n);
        } else if (found != null) {
            final long n = present(found);
            matchValue_38_10 = "found " + String.valueOf(n);
        } else {
            matchValue_38_10 = "nothing";
        }
        return matchValue_38_10;
    }

    static Either<Long, String> safeDivide(long a, long b) {
        if (b == 0L) {
            return Either.<Long, String>err("divide by zero");
        }
        return Either.<Long, String>ok(a / b);
    }

    static String quotient(long a, long b) {
        final Either<Long, String> match_54_10 = safeDivide(a, b);
        String matchValue_54_10 = "";
        if (match_54_10.isOk) {
            final long q = match_54_10.okValue();
            matchValue_54_10 = "quotient " + String.valueOf(q);
        } else if (!match_54_10.isOk) {
            final String why = match_54_10.errValue();
            matchValue_54_10 = why;
        }
        return matchValue_54_10;
    }

    static String said(ArrayList<Long> xs) {
        String matchValue_62_10 = "";
        if ((long) xs.size() == 0L) {
            matchValue_62_10 = "empty";
        } else if ((long) xs.size() == 1L) {
            final long x = xs.get(0);
            matchValue_62_10 = "just " + String.valueOf(x);
        } else {
            final long first = xs.get(0);
            final ArrayList<Long> rest = drop(xs, 1L);
            matchValue_62_10 = String.valueOf(first) + ", then " + String.valueOf((long) rest.size()) + " more";
        }
        return matchValue_62_10;
    }

    static void report(long code) {
        if (code == 200L || code == 204L) {
            System.out.println("done");
        } else if (code >= 500L) {
            System.out.println("retry " + String.valueOf(code));
        } else if (code >= 400L) {
            final long s = code;
            System.out.println("failed with " + String.valueOf(s));
        } else {
            System.out.println("? " + String.valueOf(code));
        }
    }

    static long main() {
        System.out.println(sizeOf(2L));
        System.out.println(gradeOf(85L));
        System.out.println(kindOf("main.poly"));
        System.out.println(shown(500L));
        System.out.println(quotient(7L, 2L));
        System.out.println(said(new ArrayList<>(List.of(3L, 4L, 5L))));
        report(404L);
        return 0L;
    }

    static <T> ArrayList<T> drop(ArrayList<T> xs, long n) {
        final ArrayList<T> out = new ArrayList<>();
        for (long i = n; i < (long) xs.size(); i = i + 1L) {
            out.add(xs.get((int) i));
        }
        return out;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }

    // A string's positions count its UTF-8 bytes, as on every target. A byte outside the string is 0, a slice keeps what lies
    // inside the string, and a search starts inside it.
    static byte[] utf8(String text) {
        return text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    }

    static long textLength(String text) {
        return utf8(text).length;
    }

    static byte textByte(String text, long at) {
        final byte[] bytes = utf8(text);
        return at >= 0 && at < bytes.length ? bytes[(int) at] : 0;
    }

    static String textSlice(String text, long from, long to) {
        final byte[] bytes = utf8(text);
        final int start = (int) Math.max(0, Math.min(from, bytes.length));
        final int end = (int) Math.max(start, Math.min(to, bytes.length));
        return new String(bytes, start, end - start, java.nio.charset.StandardCharsets.UTF_8);
    }

    static long textFind(String text, String part, long from) {
        final byte[] bytes = utf8(text);
        final byte[] wanted = utf8(part);
        for (int at = (int) Math.max(0, Math.min(from, bytes.length)); at + wanted.length <= bytes.length; at++) {
            if (Arrays.equals(bytes, at, at + wanted.length, wanted, 0, wanted.length)) {
                return at;
            }
        }
        return -1;
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

    // An `either`: an ok value or an error, and which one it holds.
    static final class Either<T, E> {
        final boolean isOk;
        private final Object held;

        private Either(boolean isOk, Object held) {
            this.isOk = isOk;
            this.held = held;
        }

        static <T, E> Either<T, E> ok(T value) {
            return new Either<>(true, value);
        }

        static <T, E> Either<T, E> err(E value) {
            return new Either<>(false, value);
        }

        @SuppressWarnings("unchecked")
        T okValue() {
            return (T) held;
        }

        @SuppressWarnings("unchecked")
        E errValue() {
            return (E) held;
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Either<?, ?> that && isOk == that.isOk && Objects.equals(held, that.held);
        }

        @Override
        public int hashCode() {
            return Objects.hash(isOk, held);
        }
    }
}
