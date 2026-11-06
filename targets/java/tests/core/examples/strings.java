import java.util.Arrays;
import java.util.Objects;

final class strings {
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

    static long main() {
        System.out.println("hello from polyir");
        final String name = "café";
        final long n = 3L;
        System.out.println(name + " has " + String.valueOf(textLength(name)) + " bytes; " + String.valueOf(n) + " + 1 = " + String.valueOf(n + 1L));
        System.out.println("a tab:\tand a quote: \"done\"");
        System.out.println(trim("a text block" + "\n" + "  keeps its inner indent" + "\n"));
        System.out.println(textSlice(name, 0L, 3L));
        System.out.println(String.valueOf(textFind(name, "é", 0L)));
        System.out.println(String.valueOf(textFind(name, "tea", 0L)));
        System.out.println(String.valueOf(textLength(name) * 10L));
        System.out.println(String.valueOf(Objects.equals(name, "café")));
        final byte first = textByte(name, 0L);
        System.out.println(String.valueOf((char) (first & 0xFF)));
        System.out.println(String.valueOf((char) (first & 0xFF)));
        return textLength(name) + textFind(name, "é", 0L) * 10L;
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
}
