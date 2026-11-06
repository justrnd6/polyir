import kotlin.system.exitProcess

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

fun isSpaceChar(c: Byte): Boolean {
    return c == 32.toByte() || c == 9.toByte() || c == 10.toByte() || c == 13.toByte()
}

fun trim(s: String): String {
    var from: Long = 0L
    while (from < textLength(s) && isSpaceChar(textByte(s, from))) {
        from = from + 1L
    }
    var to: Long = textLength(s)
    while (to > from && isSpaceChar(textByte(s, to - 1L))) {
        to = to - 1L
    }
    return textSlice(s, from, to)
}

fun main(): Long {
    println("hello from polyir")
    val name: String = "café"
    val n: Long = 3L
    println("${name} has ${textLength(name).toString()} bytes; ${n.toString()} + 1 = ${(n + 1L).toString()}")
    println("a tab:\tand a quote: \"done\"")
    println(trim("a text block" + "\n" + "  keeps its inner indent" + "\n"))
    println(textSlice(name, 0L, 3L))
    println(textFind(name, "é", 0L).toString())
    println(textFind(name, "tea", 0L).toString())
    println((textLength(name) * 10L).toString())
    println((name == "café").toString())
    val first: Byte = textByte(name, 0L)
    println((first.toInt() and 0xFF).toChar().toString())
    println((first.toInt() and 0xFF).toChar().toString())
    return textLength(name) + textFind(name, "é", 0L) * 10L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
