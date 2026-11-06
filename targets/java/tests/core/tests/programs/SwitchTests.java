import java.util.Arrays;
import java.util.Objects;

final class SwitchTests {
    static boolean endsWith(String s, String suffix) {
        if (textLength(suffix) > textLength(s)) {
            return false;
        }
        return Objects.equals(textSlice(s, textLength(s) - textLength(suffix), textLength(s)), suffix);
    }

    static double offText(String got, String want) {
        if (Objects.equals(got, want)) {
            return 0.0;
        }
        return 1.0;
    }

    static String dayName(long day) {
        String name = "?";
        if (day == 0L || day == 6L) {
            name = "weekend";
        } else if (day == 1L) {
            name = "monday";
        } else {
            name = "weekday";
        }
        return name;
    }

    static double checkNumbers() {
        return offText(dayName(0L) + dayName(6L) + dayName(1L) + dayName(3L), "weekendweekendmondayweekday");
    }

    static String classOf(byte c) {
        String kind = "other";
        switch (c) {
            case (byte) 97, (byte) 101, (byte) 105, (byte) 111, (byte) 117 -> {
                kind = "vowel";
            }
            case (byte) 32 -> {
                kind = "space";
            }
        }
        return kind;
    }

    static double checkChars() {
        return offText(classOf((byte) 101) + classOf((byte) 32) + classOf((byte) 122), "vowelspaceother");
    }

    static String verb(String command) {
        String done = "";
        switch (command) {
            case "start", "go" -> {
                done = "began";
            }
            case "stop" -> {
                done = "halted";
            }
            default -> {
                done = "?";
            }
        }
        return done;
    }

    static double checkStrings() {
        return offText(verb("go") + verb("start") + verb("stop") + verb("x"), "beganbeganhalted?");
    }

    static double checkBooleans() {
        String said = "";
        for (long i = 0L; i < 2L; i = i + 1L) {
            final boolean match_57_9 = i == 0L;
            if (match_57_9 == true) {
                said = said + "yes";
            } else if (match_57_9 == false) {
                said = said + "no";
            }
        }
        return offText(said, "yesno");
    }

    static String outcome(Either<Long, String> r) {
        String said = "";
        if (r.isOk) {
            final long v = r.okValue();
            said = "ok " + String.valueOf(v);
        } else if (!r.isOk) {
            final String e = r.errValue();
            said = "err " + e;
        }
        return said;
    }

    static double checkSums() {
        long count = 0L;
        final Long found = 4L;
        if (found != null) {
            final long n = present(found);
            count = n;
        } else if (found == null) {
            count = -1L;
        }
        return offText(outcome(Either.<Long, String>ok(3L)) + ", " + outcome(Either.<Long, String>err("no")) + (", " + String.valueOf(count)), "ok 3, err no, 4");
    }

    static String status(long code) {
        String said = "";
        if (code == 200L || code == 204L) {
            said = "done";
        } else if (code >= 500L) {
            said = "retry " + String.valueOf(code);
        } else if (code >= 400L) {
            final long s = code;
            said = "fail " + String.valueOf(s);
        } else {
            said = "? " + String.valueOf(code);
        }
        return said;
    }

    static double checkConditionsAmongKeys() {
        return offText(status(204L) + "; " + status(503L) + "; " + status(404L) + "; " + status(301L), "done; retry 503; fail 404; ? 301");
    }

    static long counter() {
        return 500L;
    }

    static double checkCallSubject() {
        String said = "";
        final long match_107_5 = counter();
        if (match_107_5 == 200L) {
            said = "ok";
        } else if (match_107_5 >= 500L) {
            final long s = match_107_5;
            said = "server " + String.valueOf(s);
        } else {
            said = "other";
        }
        return offText(said, "server 500");
    }

    static String step(String name) {
        String said = "";
        if (endsWith(name, ".poly")) {
            said = "compile " + name + ";";
        } else {
            said = "skip " + name + ";";
        }
        return said;
    }

    static double checkConditions() {
        return offText(step("a.poly") + step("b.md"), "compile a.poly;skip b.md;");
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
