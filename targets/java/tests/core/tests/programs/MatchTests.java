import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

final class MatchTests {
    static boolean contains(String s, String part) {
        return textFind(s, part, 0L) >= 0L;
    }

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

    static String join(ArrayList<String> parts, String sep) {
        String out = "";
        for (long i = 0L; i < (long) parts.size(); i = i + 1L) {
            if (i > 0L) {
                out = out + sep;
            }
            out = out + parts.get((int) i);
        }
        return out;
    }

    static double offText(String got, String want) {
        if (Objects.equals(got, want)) {
            return 0.0;
        }
        return 1.0;
    }

    static String sizeOf(long n) {
        String matchValue_16_12 = "";
        if (n == 0L) {
            matchValue_16_12 = "none";
        } else if (n == 1L || n == 2L || n == 3L) {
            matchValue_16_12 = "a few";
        } else if (n == -1L) {
            matchValue_16_12 = "minus one";
        } else {
            matchValue_16_12 = "many";
        }
        return matchValue_16_12;
    }

    static double checkNumbers() {
        return offText(sizeOf(0L) + sizeOf(2L) + sizeOf(9L) + sizeOf(-1L), "nonea fewmanyminus one");
    }

    static String signOf(long n) {
        String matchValue_30_16 = "";
        if (n < 0L) {
            matchValue_30_16 = "negative";
        } else if (n == 0L) {
            matchValue_30_16 = "zero";
        } else if (n >= 100L) {
            matchValue_30_16 = "large";
        } else {
            matchValue_30_16 = "positive";
        }
        final String sign = matchValue_30_16;
        return sign;
    }

    static double checkComparisons() {
        return offText(signOf(-3L) + " " + signOf(0L) + " " + signOf(150L) + " " + signOf(5L), "negative zero large positive");
    }

    static String gradeOf(long score) {
        String matchValue_45_12 = "";
        if (score == 100L) {
            matchValue_45_12 = "perfect";
        } else if (score >= 90L) {
            final long s = score;
            matchValue_45_12 = "A " + String.valueOf(s);
        } else if (score >= 80L) {
            matchValue_45_12 = "B";
        } else {
            matchValue_45_12 = "C";
        }
        return matchValue_45_12;
    }

    static double checkGuardsOnTheSubject() {
        return offText(gradeOf(100L) + gradeOf(93L) + gradeOf(85L) + gradeOf(10L), "perfectA 93BC");
    }

    static boolean yes(String reply) {
        boolean matchValue_60_12 = false;
        switch (reply) {
            case "y", "yes" -> {
                matchValue_60_12 = true;
            }
            case "n", "no" -> {
                matchValue_60_12 = false;
            }
            default -> {
                matchValue_60_12 = false;
            }
        }
        return matchValue_60_12;
    }

    static double checkStrings() {
        return offText(String.valueOf(yes("y")) + " " + String.valueOf(yes("yes")) + " " + String.valueOf(yes("no")) + " " + String.valueOf(yes("maybe")), "true true false false");
    }

    static String kindOf(String path) {
        String matchValue_73_12 = "";
        if (Objects.equals(path, "Ninafile") || Objects.equals(path, "Polyfile")) {
            matchValue_73_12 = "build";
        } else if (endsWith(path, ".poly")) {
            matchValue_73_12 = "polyir";
        } else if (startsWith(path, "README")) {
            matchValue_73_12 = "docs";
        } else if (contains(path, "test") && !endsWith(path, ".md")) {
            matchValue_73_12 = "a test";
        } else if (endsWith(path, ".md")) {
            final String p = path;
            matchValue_73_12 = "markdown " + p;
        } else {
            matchValue_73_12 = "other";
        }
        return matchValue_73_12;
    }

    static double checkConditions() {
        final ArrayList<String> kinds = new ArrayList<>(List.of(kindOf("Polyfile"), kindOf("a.poly"), kindOf("README.md"), kindOf("my_test.c"), kindOf("notes.md"), kindOf("x.c")));
        return offText(join(kinds, ","), "build,polyir,docs,a test,markdown notes.md,other");
    }

    static String word(boolean ready) {
        String matchValue_89_12 = "";
        if (ready == true) {
            matchValue_89_12 = "go";
        } else if (ready == false) {
            matchValue_89_12 = "wait";
        }
        return matchValue_89_12;
    }

    static double checkBooleans() {
        return offText(word(true) + word(false), "gowait");
    }

    static String shown(Long found) {
        String matchValue_102_12 = "";
        if (found != null && present(found) > 100L) {
            final long n = present(found);
            matchValue_102_12 = "big " + String.valueOf(n);
        } else if (found != null) {
            final long n = present(found);
            matchValue_102_12 = "found " + String.valueOf(n);
        } else {
            matchValue_102_12 = "nothing";
        }
        return matchValue_102_12;
    }

    static double checkOptionals() {
        return offText(shown(500L) + ", " + shown(5L) + ", " + shown(null), "big 500, found 5, nothing");
    }

    static Either<Long, String> safeDivide(long a, long b) {
        if (b == 0L) {
            return Either.<Long, String>err("divide by zero");
        }
        return Either.<Long, String>ok(a / b);
    }

    static String message(long a, long b) {
        final Either<Long, String> match_122_12 = safeDivide(a, b);
        String matchValue_122_12 = "";
        if (match_122_12.isOk) {
            final long q = match_122_12.okValue();
            matchValue_122_12 = "quotient " + String.valueOf(q);
        } else if (!match_122_12.isOk) {
            final String why = match_122_12.errValue();
            matchValue_122_12 = "error " + why;
        }
        return matchValue_122_12;
    }

    static double checkEithers() {
        return offText(message(7L, 2L) + "; " + message(7L, 0L), "quotient 3; error divide by zero");
    }

    static String textOf(Variant shape) {
        String matchValue_133_12 = "";
        if (shape.tag() == 0L) {
            final long r = (Long) shape.value();
            matchValue_133_12 = "circle " + String.valueOf(r);
        } else if (shape.tag() == 1L) {
            final long s = (Long) shape.value();
            matchValue_133_12 = "square " + String.valueOf(s);
        } else if (shape.tag() == 2L) {
            final String name = (String) shape.value();
            matchValue_133_12 = name;
        }
        return matchValue_133_12;
    }

    static double checkVariants() {
        final String circle = textOf(new Variant(0L, (long) (2L)));
        return offText(circle + ", " + textOf(new Variant(1L, (long) (4L))) + ", " + textOf(new Variant(2L, "blob")), "circle 2, square 4, blob");
    }

    static String zeroOr(Long found) {
        String matchValue_147_12 = "";
        if (found != null && present(found) == 0L) {
            matchValue_147_12 = "zero";
        } else if (found != null) {
            matchValue_147_12 = "other";
        } else {
            matchValue_147_12 = "none";
        }
        return matchValue_147_12;
    }

    static double checkPayloadPatterns() {
        return offText(zeroOr(0L) + zeroOr(3L) + zeroOr(null), "zeroothernone");
    }

    static String said(ArrayList<Long> xs) {
        String matchValue_161_12 = "";
        if ((long) xs.size() == 0L) {
            matchValue_161_12 = "empty";
        } else if ((long) xs.size() == 1L) {
            final long x = xs.get(0);
            matchValue_161_12 = "just " + String.valueOf(x);
        } else if (xs.get(0) == 0L) {
            matchValue_161_12 = "starts at zero";
        } else {
            final long first = xs.get(0);
            final ArrayList<Long> rest = drop(xs, 1L);
            matchValue_161_12 = String.valueOf(first) + ", then " + String.valueOf((long) rest.size()) + " more";
        }
        return matchValue_161_12;
    }

    static double checkLists() {
        final ArrayList<Long> none = new ArrayList<>();
        return offText(said(none) + "; " + said(new ArrayList<>(List.of(7L))) + "; " + said(new ArrayList<>(List.of(0L, 1L, 2L))) + "; " + said(new ArrayList<>(List.of(3L, 4L, 5L))), "empty; just 7; starts at zero; 3, then 2 more");
    }

    static String pairOf(ArrayList<Long> xs) {
        String matchValue_176_12 = "";
        if ((long) xs.size() == 2L && xs.get(0) == xs.get(1)) {
            final long a = xs.get(0);
            matchValue_176_12 = "a pair of " + String.valueOf(a);
        } else if ((long) xs.size() == 2L) {
            final long a = xs.get(0);
            final long b = xs.get(1);
            matchValue_176_12 = String.valueOf(a) + " and " + String.valueOf(b);
        } else if ((long) xs.size() >= 1L && xs.get(0) < 0L) {
            matchValue_176_12 = "starts below zero";
        } else {
            matchValue_176_12 = "something else";
        }
        return matchValue_176_12;
    }

    static double checkListGuards() {
        final ArrayList<Long> same = new ArrayList<>(List.of(4L, 4L));
        final ArrayList<Long> two = new ArrayList<>(List.of(1L, 2L));
        final ArrayList<Long> low = new ArrayList<>(List.of(-1L, 2L, 3L));
        return offText(pairOf(same) + "; " + pairOf(two) + "; " + pairOf(low) + "; " + pairOf(new ArrayList<>(List.of(5L))), "a pair of 4; 1 and 2; starts below zero; something else");
    }

    static String command(String c) {
        String said = "";
        if (Objects.equals(c, "quit") || Objects.equals(c, "exit")) {
            return "bye";
        } else if (startsWith(c, "say ")) {
            said = textSlice(c, 4L, textLength(c));
        } else if (Objects.equals(c, "noop")) {}
        return said;
    }

    static double checkStatements() {
        return offText(command("say hi") + "|" + command("quit") + "|" + command("noop") + "|" + command("huh"), "hi|bye||");
    }

    static double checkNested() {
        final long a = 2L;
        final long b = 5L;
        String matchValue_212_21 = "";
        if (a == 1L) {
            matchValue_212_21 = "one";
        } else if (a == 2L) {
            String matchValue_212_49 = "";
            if (b == 5L) {
                matchValue_212_49 = "two-five";
            } else {
                matchValue_212_49 = "two";
            }
            matchValue_212_21 = matchValue_212_49;
        } else {
            matchValue_212_21 = "many";
        }
        final String text = matchValue_212_21;
        return offText(text, "two-five");
    }

    static <T> ArrayList<T> drop(ArrayList<T> xs, long n) {
        final ArrayList<T> out = new ArrayList<>();
        for (long i = n; i < (long) xs.size(); i = i + 1L) {
            out.add(xs.get((int) i));
        }
        return out;
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

    // A variant: the number of its case and the value it holds.
    record Variant(long tag, Object value) {}
}
