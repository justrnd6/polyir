import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Objects;

final class Sets {
    record Point(long x, long y) {
    }

    record Named(String name, String tag) {
    }

    static final class Cell2 {
        long v;
        String note;

        Cell2(long v, String note) {
            this.v = v;
            this.note = note;
        }

        public long bump() {
            this.v = this.v + 1L;
            return this.v;
        }

        Cell2 copy() {
            return new Cell2(v, note);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Cell2 that && Arrays.equals(new Object[] {v, note}, new Object[] {that.v, that.note});
        }

        @Override
        public int hashCode() {
            return Objects.hash(v, note);
        }
    }

    static long double_(long x) {
        return x * 2L;
    }

    static long inc(long x) {
        return x + 1L;
    }

    static long sets() {
        final HashSet<Point> seen = new HashSet<>();
        seen.add(new Point(1L, 2L));
        seen.add(new Point(3L, 4L));
        seen.add(new Point(1L, 2L));
        long score = (long) seen.size();
        if (seen.contains(new Point(3L, 4L))) {
            score = score + 10L;
        }
        final HashSet<String> words = new HashSet<>();
        words.add("a");
        words.add("b");
        words.add("a");
        score = score + (long) words.size() * 100L;
        return score;
    }

    static long maps() {
        final HashMap<String, Long> m = new HashMap<>();
        m.put("x", 5L);
        m.put("y", 6L);
        m.put("x", m.getOrDefault("x", 0L) + 1L);
        final HashMap<Point, String> byPoint = new HashMap<>();
        byPoint.put(new Point(1L, 1L), "p");
        long score = m.getOrDefault("x", 0L) * 10L + m.getOrDefault("y", 0L) + (long) m.size();
        if (byPoint.containsKey(new Point(1L, 1L))) {
            score = score + 100L;
        }
        if (m.getOrDefault("none", 0L) == 0L) {
            score = score + 1000L;
        }
        return score;
    }

    static long strs() {
        final String a = "ab";
        final String b = a + "cd" + a;
        long score = textLength(b);
        if (Objects.equals(b, "abcdab")) {
            score = score + 10L;
        }
        if (!Objects.equals(b, a)) {
            score = score + 100L;
        }
        final String q = "say hi {x}";
        score = score + textLength(q);
        return score;
    }

    static long opts() {
        final Named n = new Named("a", "t");
        final Named m = new Named("a", "t");
        final Named o = new Named("a", null);
        long score = 0L;
        if (Objects.equals(n, m)) {
            score = score + 1L;
        }
        if (Objects.equals(n, o)) {
            score = score + 10L;
        }
        if (o.tag == null) {
            score = score + 100L;
        }
        return score + textLength(present(n.tag));
    }

    static long bufs() {
        final long[] b = new long[4];
        b[0] = 7L;
        b[3] = 9L;
        long s = 0L;
        for (long i = 0L; i < 4L; i = i + 1L) {
            s = s + b[(int) i];
        }
        return s;
    }

    static long swaps() {
        final Cell2 c = new Cell2(1L, "n");
        final Cell2 d = c;
        d.bump();
        c.note = c.note + "!";
        if (Objects.equals(d.note, "n!")) {
            return c.v * 10L + textLength(d.note);
        }
        return 0L;
    }

    static long choose(long k) {
        long label = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    if (k == 0L) {
                        label = 1L;
                        continue machine;
                    } else if (k == 1L) {
                        label = 2L;
                        continue machine;
                    } else {
                        label = 3L;
                        continue machine;
                    }
                }
                case 1: {
                    return 100L;
                }
                case 2: {
                    return 200L;
                }
                case 3: {
                    return 300L;
                }
            }
        }
    }

    static long loops(long n) {
        long s = 0L;
        {
            long i = 0L;
            long j = n;
            while (i < j) {
                s = s + i * j;
                i = i + 1L;
                j = j - 1L;
            }
        }
        long k = 0L;
        for (long t = 0L; t < 4L; t = t + 1L) {
            k = k + 1L;
        }
        return s + k;
    }

    static long unsigned() {
        final int a = 250;
        final int b = (a + 10) & 0xFF;
        final long c = 4000000000L;
        final long d = (c + c) & 0xFFFFFFFFL;
        final int e = 2147483647;
        final int f = e + 1;
        return (long) b + (long) d + (long) f % 1000L;
    }

    static long floats() {
        final double x = 7.5;
        final double y = 2.25;
        final double z = x / 2.0 + y * 2.0;
        if (z > 8.0 && x != y) {
            return (long) (double) z;
        }
        return 0L;
    }

    static long main() {
        long score = 0L;
        score = score + sets() + maps() + strs() + opts() + bufs() + swaps();
        score = score + choose(0L) + choose(1L) + choose(5L) + loops(6L) + unsigned() + floats();
        return score % 251L;
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
}
