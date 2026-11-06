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
