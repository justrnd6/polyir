import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Objects;

final class Text {
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

    static ArrayList<String> split(String s, String sep) {
        final ArrayList<String> parts = new ArrayList<>();
        if (textLength(sep) == 0L) {
            parts.add(s);
            return parts;
        }
        long from = 0L;
        for (long at = textFind(s, sep, 0L); at >= 0L; at = textFind(s, sep, from)) {
            parts.add(textSlice(s, from, at));
            from = at + textLength(sep);
        }
        parts.add(textSlice(s, from, textLength(s)));
        return parts;
    }

    static ArrayList<String> lines(String s) {
        return split(s, "\n");
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

    static boolean isSpaceChar(byte c) {
        return c == (byte) 32 || c == (byte) 9 || c == (byte) 10 || c == (byte) 13;
    }

    static String trim(String s) {
        long from = 0L;
        while (from < textLength(s) && isSpaceChar(textByte(s, from))) {
            from = from + 1L;
        }
        long to = textLength(s);
        while (to > from && isSpaceChar(textByte(s, to - 1L))) {
            to = to - 1L;
        }
        return textSlice(s, from, to);
    }

    static String replace(String s, String old, String by) {
        return join(split(s, old), by);
    }

    static String repeat(String s, long n) {
        String out = "";
        for (long i = 0L; i < n; i = i + 1L) {
            out = out + s;
        }
        return out;
    }

    static String indent(String s, long n) {
        final ArrayList<String> all = lines(s);
        final String pad = repeat(" ", n);
        for (long i = 0L; i < (long) all.size(); i = i + 1L) {
            if (textLength(all.get((int) i)) > 0L) {
                all.set((int) i, pad + all.get((int) i));
            }
        }
        return join(all, "\n");
    }

    static String indentRest(String s, long n) {
        final long first = textFind(s, "\n", 0L);
        if (first < 0L) {
            return s;
        }
        return textSlice(s, 0L, first + 1L) + indent(textSlice(s, first + 1L, textLength(s)), n);
    }

    static String fill(String template, HashMap<String, String> values) {
        String out = "";
        long from = 0L;
        for (long at = textFind(template, "${", 0L); at >= 0L; at = textFind(template, "${", from)) {
            final long close = textFind(template, "}", at);
            if (close < 0L) {
                return out + textSlice(template, from, textLength(template));
            }
            out = out + textSlice(template, from, at) + filledName(textSlice(template, at, close + 1L), values);
            from = close + 1L;
        }
        return out + textSlice(template, from, textLength(template));
    }

    static String filledName(String placeholder, HashMap<String, String> values) {
        final String name = trim(textSlice(placeholder, 2L, textLength(placeholder) - 1L));
        if (values.containsKey(name)) {
            return values.getOrDefault(name, "");
        }
        return placeholder;
    }

    record StringBuilder(ArrayList<String> pieces, ArrayList<Long> indents) {
        public StringBuilder add(String s) {
            this.pieces.add(s);
            return this;
        }

        public StringBuilder line(String s) {
            if (textLength(s) > 0L) {
                this.pieces.add(repeat(" ", this.indentNow()));
            }
            this.pieces.add(s);
            this.pieces.add("\n");
            return this;
        }

        public StringBuilder blank() {
            this.pieces.add("\n");
            return this;
        }

        public StringBuilder lines(ArrayList<String> all) {
            for (long i = 0L; i < (long) all.size(); i = i + 1L) {
                this.line(all.get((int) i));
            }
            return this;
        }

        public StringBuilder indent(long n) {
            this.indents.add(this.indentNow() + n);
            return this;
        }

        public StringBuilder dedent() {
            if ((long) this.indents.size() > 0L) {
                truncate(this.indents, (long) this.indents.size() - 1L);
            }
            return this;
        }

        public long indentNow() {
            if ((long) this.indents.size() == 0L) {
                return 0L;
            }
            return this.indents.get((int) ((long) this.indents.size() - 1L));
        }

        public long length() {
            return textLength(this.text());
        }

        public boolean isEmpty() {
            return this.length() == 0L;
        }

        public String text() {
            return join(this.pieces, "");
        }
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

    // Shortens a list to at most `count` items, and gives it back.
    static <T> ArrayList<T> truncate(ArrayList<T> items, long count) {
        while (items.size() > Math.max(count, 0)) {
            items.remove(items.size() - 1);
        }
        return items;
    }
}
