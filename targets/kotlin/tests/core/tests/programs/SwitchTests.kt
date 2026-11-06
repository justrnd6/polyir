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

fun endsWith(s: String, suffix: String): Boolean {
    if (textLength(suffix) > textLength(s)) {
        return false
    }
    return textSlice(s, textLength(s) - textLength(suffix), textLength(s)) == suffix
}

fun offText(got: String, want: String): Double {
    if (got == want) {
        return 0.0
    }
    return 1.0
}

fun dayName(day: Long): String {
    var name: String = "?"
    when (day) {
        0L, 6L -> {
            name = "weekend"
        }
        1L -> {
            name = "monday"
        }
        else -> {
            name = "weekday"
        }
    }
    return name
}

fun checkNumbers(): Double {
    return offText(dayName(0L) + dayName(6L) + dayName(1L) + dayName(3L), "weekendweekendmondayweekday")
}

fun classOf(c: Byte): String {
    var kind: String = "other"
    when (c) {
        97.toByte(), 101.toByte(), 105.toByte(), 111.toByte(), 117.toByte() -> {
            kind = "vowel"
        }
        32.toByte() -> {
            kind = "space"
        }
    }
    return kind
}

fun checkChars(): Double {
    return offText(classOf(101.toByte()) + classOf(32.toByte()) + classOf(122.toByte()), "vowelspaceother")
}

fun verb(command: String): String {
    var done: String = ""
    when (command) {
        "start", "go" -> {
            done = "began"
        }
        "stop" -> {
            done = "halted"
        }
        else -> {
            done = "?"
        }
    }
    return done
}

fun checkStrings(): Double {
    return offText(verb("go") + verb("start") + verb("stop") + verb("x"), "beganbeganhalted?")
}

fun checkBooleans(): Double {
    var said: String = ""
    run {
        var i: Long = 0L
        while (i < 2L) {
            val match_57_9: Boolean = i == 0L
            when (match_57_9) {
                true -> {
                    said = said + "yes"
                }
                false -> {
                    said = said + "no"
                }
            }
            i = i + 1L
        }
    }
    return offText(said, "yesno")
}

fun outcome(r: Either<Long, String>): String {
    var said: String = ""
    if (r.isOk) {
        val v: Long = r.okValue
        said = "ok ${v}"
    } else if (!r.isOk) {
        val e: String = r.errValue
        said = "err ${e}"
    }
    return said
}

fun checkSums(): Double {
    var count: Long = 0L
    val found: Long? = 4L
    if (found != null) {
        val n: Long = found ?: fail("unwrap of an empty optional")
        count = n
    } else if (found == null) {
        count = -1L
    }
    return offText("${outcome(Either.ok<Long, String>(3L))}, ${outcome(Either.err<Long, String>("no"))}, ${count}", "ok 3, err no, 4")
}

fun status(code: Long): String {
    var said: String = ""
    if (code == 200L || code == 204L) {
        said = "done"
    } else if (code >= 500L) {
        said = "retry ${code}"
    } else if (code >= 400L) {
        val s: Long = code
        said = "fail ${s}"
    } else {
        said = "? ${code}"
    }
    return said
}

fun checkConditionsAmongKeys(): Double {
    return offText(status(204L) + "; " + status(503L) + "; " + status(404L) + "; " + status(301L), "done; retry 503; fail 404; ? 301")
}

fun counter(): Long {
    return 500L
}

fun checkCallSubject(): Double {
    var said: String = ""
    val match_107_5: Long = counter()
    if (match_107_5 == 200L) {
        said = "ok"
    } else if (match_107_5 >= 500L) {
        val s: Long = match_107_5
        said = "server ${s}"
    } else {
        said = "other"
    }
    return offText(said, "server 500")
}

fun step(name: String): String {
    var said: String = ""
    if (endsWith(name, ".poly")) {
        said = "compile " + name + ";"
    } else {
        said = "skip " + name + ";"
    }
    return said
}

fun checkConditions(): Double {
    return offText(step("a.poly") + step("b.md"), "compile a.poly;skip b.md;")
}
