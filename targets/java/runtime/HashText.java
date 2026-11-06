// A string's hash: djb2 over its UTF-8 bytes, wrapping at 64 bits, as the native code computes it.
static long hashText(String text) {
    long hash = 5381;
    for (final byte b : text.getBytes(java.nio.charset.StandardCharsets.UTF_8)) {
        hash = hash * 33 + (b & 0xFF);
    }
    return hash;
}
