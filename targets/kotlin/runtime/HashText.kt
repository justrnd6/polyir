// A string's hash: djb2 over its UTF-8 bytes, wrapping at 64 bits, as the native code computes it.
private fun hashText(text: String): Long {
    var hash = 5381L
    for (byte in text.toByteArray()) {
        hash = hash * 33 + (byte.toInt() and 0xFF)
    }
    return hash
}
