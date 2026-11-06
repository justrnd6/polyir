// A string's positions count its UTF-8 bytes, as on every target. A byte outside the string is 0, a slice keeps what lies
// inside the string, and a search starts inside it.
private fun textLength(text: String): Long = text.toByteArray().size.toLong()

private fun textByte(text: String, at: Long): Byte {
    val bytes = text.toByteArray()
    return if (at >= 0 && at < bytes.size) bytes[at.toInt()] else 0
}

private fun textSlice(text: String, from: Long, to: Long): String {
    val bytes = text.toByteArray()
    val start = from.coerceIn(0L, bytes.size.toLong()).toInt()
    val end = to.coerceIn(start.toLong(), bytes.size.toLong()).toInt()
    return String(bytes, start, end - start)
}

private fun textFind(text: String, part: String, from: Long): Long {
    val bytes = text.toByteArray()
    val wanted = part.toByteArray()
    var at = from.coerceIn(0L, bytes.size.toLong()).toInt()
    while (at + wanted.size <= bytes.size) {
        if (wanted.indices.all { bytes[at + it] == wanted[it] }) {
            return at.toLong()
        }
        at++
    }
    return -1L
}
