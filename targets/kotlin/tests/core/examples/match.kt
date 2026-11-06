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

// Stops the program with a message, as polyir's `fail` does.
private fun fail(message: String): Nothing {
    System.err.println("trap: " + message)
    exitProcess(1)
}

// An `either`: an ok value or an error, and which one it holds.
class Either<T, E> private constructor(val isOk: Boolean, private val held: Any?) {
    @Suppress("UNCHECKED_CAST")
    val okValue: T get() = held as T

    @Suppress("UNCHECKED_CAST")
    val errValue: E get() = held as E

    override fun equals(other: Any?): Boolean = other is Either<*, *> && isOk == other.isOk && held == other.held

    override fun hashCode(): Int = 31 * isOk.hashCode() + (held?.hashCode() ?: 0)

    companion object {
        fun <T, E> ok(value: T): Either<T, E> = Either(true, value)

        fun <T, E> err(value: E): Either<T, E> = Either(false, value)
    }
}

fun startsWith(s: String, prefix: String): Boolean {
    if (textLength(prefix) > textLength(s)) {
        return false
    }
    return textSlice(s, 0L, textLength(prefix)) == prefix
}

fun endsWith(s: String, suffix: String): Boolean {
    if (textLength(suffix) > textLength(s)) {
        return false
    }
    return textSlice(s, textLength(s) - textLength(suffix), textLength(s)) == suffix
}

fun sizeOf(n: Long): String {
    var matchValue_10_10: String = ""
    when (n) {
        0L -> {
            matchValue_10_10 = "none"
        }
        1L, 2L, 3L -> {
            matchValue_10_10 = "a few"
        }
        else -> {
            matchValue_10_10 = "many"
        }
    }
    return matchValue_10_10
}

fun gradeOf(score: Long): String {
    var matchValue_19_10: String = ""
    if (score >= 90L) {
        matchValue_19_10 = "A"
    } else if (score >= 80L) {
        matchValue_19_10 = "B"
    } else {
        matchValue_19_10 = "C"
    }
    return matchValue_19_10
}

fun kindOf(path: String): String {
    var matchValue_28_10: String = ""
    if (path == "Ninafile" || path == "Polyfile") {
        matchValue_28_10 = "build"
    } else if (endsWith(path, ".poly")) {
        matchValue_28_10 = "polyir"
    } else if (startsWith(path, "README")) {
        matchValue_28_10 = "docs"
    } else {
        matchValue_28_10 = "other"
    }
    return matchValue_28_10
}

fun shown(found: Long?): String {
    var matchValue_38_10: String = ""
    if (found != null && found ?: fail("unwrap of an empty optional") > 100L) {
        val n: Long = found ?: fail("unwrap of an empty optional")
        matchValue_38_10 = "big ${n}"
    } else if (found != null) {
        val n: Long = found ?: fail("unwrap of an empty optional")
        matchValue_38_10 = "found ${n}"
    } else {
        matchValue_38_10 = "nothing"
    }
    return matchValue_38_10
}

fun safeDivide(a: Long, b: Long): Either<Long, String> {
    if (b == 0L) {
        return Either.err<Long, String>("divide by zero")
    }
    return Either.ok<Long, String>(a / b)
}

fun quotient(a: Long, b: Long): String {
    val match_54_10: Either<Long, String> = safeDivide(a, b)
    var matchValue_54_10: String = ""
    if (match_54_10.isOk) {
        val q: Long = match_54_10.okValue
        matchValue_54_10 = "quotient ${q}"
    } else if (!match_54_10.isOk) {
        val why: String = match_54_10.errValue
        matchValue_54_10 = why
    }
    return matchValue_54_10
}

fun said(xs: MutableList<Long>): String {
    var matchValue_62_10: String = ""
    if (xs.size.toLong() == 0L) {
        matchValue_62_10 = "empty"
    } else if (xs.size.toLong() == 1L) {
        val x: Long = xs[0]
        matchValue_62_10 = "just ${x}"
    } else {
        val first: Long = xs[0]
        val rest: MutableList<Long> = drop(xs, 1L)
        matchValue_62_10 = "${first}, then ${rest.size.toLong()} more"
    }
    return matchValue_62_10
}

fun report(code: Long): Unit {
    if (code == 200L || code == 204L) {
        println("done")
    } else if (code >= 500L) {
        println("retry ${code}")
    } else if (code >= 400L) {
        val s: Long = code
        println("failed with ${s}")
    } else {
        println("? ${code}")
    }
}

fun main(): Long {
    println(sizeOf(2L))
    println(gradeOf(85L))
    println(kindOf("main.poly"))
    println(shown(500L))
    println(quotient(7L, 2L))
    println(said(mutableListOf(3L, 4L, 5L)))
    report(404L)
    return 0L
}

fun <T> drop(xs: MutableList<T>, n: Long): MutableList<T> {
    val out: MutableList<T> = mutableListOf()
    run {
        var i: Long = n
        while (i < xs.size.toLong()) {
            out.add(xs[i.toInt()])
            i = i + 1L
        }
    }
    return out
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
