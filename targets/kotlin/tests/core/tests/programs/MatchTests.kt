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

// A variant: the number of its case and the value it holds.
data class Variant(val tag: Long, val value: Any?)

fun contains(s: String, part: String): Boolean {
    return textFind(s, part, 0L) >= 0L
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

fun join(parts: MutableList<String>, sep: String): String {
    var out: String = ""
    run {
        var i: Long = 0L
        while (i < parts.size.toLong()) {
            if (i > 0L) {
                out = out + sep
            }
            out = out + parts[i.toInt()]
            i = i + 1L
        }
    }
    return out
}

fun offText(got: String, want: String): Double {
    if (got == want) {
        return 0.0
    }
    return 1.0
}

fun sizeOf(n: Long): String {
    var matchValue_16_12: String = ""
    when (n) {
        0L -> {
            matchValue_16_12 = "none"
        }
        1L, 2L, 3L -> {
            matchValue_16_12 = "a few"
        }
        -1L -> {
            matchValue_16_12 = "minus one"
        }
        else -> {
            matchValue_16_12 = "many"
        }
    }
    return matchValue_16_12
}

fun checkNumbers(): Double {
    return offText(sizeOf(0L) + sizeOf(2L) + sizeOf(9L) + sizeOf(-1L), "nonea fewmanyminus one")
}

fun signOf(n: Long): String {
    var matchValue_30_16: String = ""
    if (n < 0L) {
        matchValue_30_16 = "negative"
    } else if (n == 0L) {
        matchValue_30_16 = "zero"
    } else if (n >= 100L) {
        matchValue_30_16 = "large"
    } else {
        matchValue_30_16 = "positive"
    }
    val sign: String = matchValue_30_16
    return sign
}

fun checkComparisons(): Double {
    return offText(signOf(-3L) + " " + signOf(0L) + " " + signOf(150L) + " " + signOf(5L), "negative zero large positive")
}

fun gradeOf(score: Long): String {
    var matchValue_45_12: String = ""
    if (score == 100L) {
        matchValue_45_12 = "perfect"
    } else if (score >= 90L) {
        val s: Long = score
        matchValue_45_12 = "A ${s}"
    } else if (score >= 80L) {
        matchValue_45_12 = "B"
    } else {
        matchValue_45_12 = "C"
    }
    return matchValue_45_12
}

fun checkGuardsOnTheSubject(): Double {
    return offText(gradeOf(100L) + gradeOf(93L) + gradeOf(85L) + gradeOf(10L), "perfectA 93BC")
}

fun yes(reply: String): Boolean {
    var matchValue_60_12: Boolean = false
    when (reply) {
        "y", "yes" -> {
            matchValue_60_12 = true
        }
        "n", "no" -> {
            matchValue_60_12 = false
        }
        else -> {
            matchValue_60_12 = false
        }
    }
    return matchValue_60_12
}

fun checkStrings(): Double {
    return offText("${yes("y")} ${yes("yes")} ${yes("no")} ${yes("maybe")}", "true true false false")
}

fun kindOf(path: String): String {
    var matchValue_73_12: String = ""
    if (path == "Ninafile" || path == "Polyfile") {
        matchValue_73_12 = "build"
    } else if (endsWith(path, ".poly")) {
        matchValue_73_12 = "polyir"
    } else if (startsWith(path, "README")) {
        matchValue_73_12 = "docs"
    } else if (contains(path, "test") && !endsWith(path, ".md")) {
        matchValue_73_12 = "a test"
    } else if (endsWith(path, ".md")) {
        val p: String = path
        matchValue_73_12 = "markdown ${p}"
    } else {
        matchValue_73_12 = "other"
    }
    return matchValue_73_12
}

fun checkConditions(): Double {
    val kinds: MutableList<String> = mutableListOf(kindOf("Polyfile"), kindOf("a.poly"), kindOf("README.md"), kindOf("my_test.c"), kindOf("notes.md"), kindOf("x.c"))
    return offText(join(kinds, ","), "build,polyir,docs,a test,markdown notes.md,other")
}

fun word(ready: Boolean): String {
    var matchValue_89_12: String = ""
    when (ready) {
        true -> {
            matchValue_89_12 = "go"
        }
        false -> {
            matchValue_89_12 = "wait"
        }
    }
    return matchValue_89_12
}

fun checkBooleans(): Double {
    return offText(word(true) + word(false), "gowait")
}

fun shown(found: Long?): String {
    var matchValue_102_12: String = ""
    if (found != null && found ?: fail("unwrap of an empty optional") > 100L) {
        val n: Long = found ?: fail("unwrap of an empty optional")
        matchValue_102_12 = "big ${n}"
    } else if (found != null) {
        val n: Long = found ?: fail("unwrap of an empty optional")
        matchValue_102_12 = "found ${n}"
    } else {
        matchValue_102_12 = "nothing"
    }
    return matchValue_102_12
}

fun checkOptionals(): Double {
    return offText(shown(500L) + ", " + shown(5L) + ", " + shown(null), "big 500, found 5, nothing")
}

fun safeDivide(a: Long, b: Long): Either<Long, String> {
    if (b == 0L) {
        return Either.err<Long, String>("divide by zero")
    }
    return Either.ok<Long, String>(a / b)
}

fun message(a: Long, b: Long): String {
    val match_122_12: Either<Long, String> = safeDivide(a, b)
    var matchValue_122_12: String = ""
    if (match_122_12.isOk) {
        val q: Long = match_122_12.okValue
        matchValue_122_12 = "quotient ${q}"
    } else if (!match_122_12.isOk) {
        val why: String = match_122_12.errValue
        matchValue_122_12 = "error ${why}"
    }
    return matchValue_122_12
}

fun checkEithers(): Double {
    return offText(message(7L, 2L) + "; " + message(7L, 0L), "quotient 3; error divide by zero")
}

fun textOf(shape: Variant): String {
    var matchValue_133_12: String = ""
    if (shape.tag == 0L) {
        val r: Long = shape.value as Long
        matchValue_133_12 = "circle ${r}"
    } else if (shape.tag == 1L) {
        val s: Long = shape.value as Long
        matchValue_133_12 = "square ${s}"
    } else if (shape.tag == 2L) {
        val name: String = shape.value as String
        matchValue_133_12 = name
    }
    return matchValue_133_12
}

fun checkVariants(): Double {
    val circle: String = textOf(Variant(0L, (2L).toLong()))
    return offText(circle + ", " + textOf(Variant(1L, (4L).toLong())) + ", " + textOf(Variant(2L, "blob")), "circle 2, square 4, blob")
}

fun zeroOr(found: Long?): String {
    var matchValue_147_12: String = ""
    if (found != null && found ?: fail("unwrap of an empty optional") == 0L) {
        matchValue_147_12 = "zero"
    } else if (found != null) {
        matchValue_147_12 = "other"
    } else {
        matchValue_147_12 = "none"
    }
    return matchValue_147_12
}

fun checkPayloadPatterns(): Double {
    return offText(zeroOr(0L) + zeroOr(3L) + zeroOr(null), "zeroothernone")
}

fun said(xs: MutableList<Long>): String {
    var matchValue_161_12: String = ""
    if (xs.size.toLong() == 0L) {
        matchValue_161_12 = "empty"
    } else if (xs.size.toLong() == 1L) {
        val x: Long = xs[0]
        matchValue_161_12 = "just ${x}"
    } else if (xs[0] == 0L) {
        matchValue_161_12 = "starts at zero"
    } else {
        val first: Long = xs[0]
        val rest: MutableList<Long> = drop(xs, 1L)
        matchValue_161_12 = "${first}, then ${rest.size.toLong()} more"
    }
    return matchValue_161_12
}

fun checkLists(): Double {
    val none: MutableList<Long> = mutableListOf()
    return offText(said(none) + "; " + said(mutableListOf(7L)) + "; " + said(mutableListOf(0L, 1L, 2L)) + "; " + said(mutableListOf(3L, 4L, 5L)), "empty; just 7; starts at zero; 3, then 2 more")
}

fun pairOf(xs: MutableList<Long>): String {
    var matchValue_176_12: String = ""
    if (xs.size.toLong() == 2L && xs[0] == xs[1]) {
        val a: Long = xs[0]
        matchValue_176_12 = "a pair of ${a}"
    } else if (xs.size.toLong() == 2L) {
        val a: Long = xs[0]
        val b: Long = xs[1]
        matchValue_176_12 = "${a} and ${b}"
    } else if (xs.size.toLong() >= 1L && xs[0] < 0L) {
        matchValue_176_12 = "starts below zero"
    } else {
        matchValue_176_12 = "something else"
    }
    return matchValue_176_12
}

fun checkListGuards(): Double {
    val same: MutableList<Long> = mutableListOf(4L, 4L)
    val two: MutableList<Long> = mutableListOf(1L, 2L)
    val low: MutableList<Long> = mutableListOf(-1L, 2L, 3L)
    return offText(pairOf(same) + "; " + pairOf(two) + "; " + pairOf(low) + "; " + pairOf(mutableListOf(5L)), "a pair of 4; 1 and 2; starts below zero; something else")
}

fun command(c: String): String {
    var said: String = ""
    if (c == "quit" || c == "exit") {
        return "bye"
    } else if (startsWith(c, "say ")) {
        said = textSlice(c, 4L, textLength(c))
    } else if (c == "noop") {}
    return said
}

fun checkStatements(): Double {
    return offText(command("say hi") + "|" + command("quit") + "|" + command("noop") + "|" + command("huh"), "hi|bye||")
}

fun checkNested(): Double {
    val a: Long = 2L
    val b: Long = 5L
    var matchValue_212_21: String = ""
    when (a) {
        1L -> {
            matchValue_212_21 = "one"
        }
        2L -> {
            var matchValue_212_49: String = ""
            when (b) {
                5L -> {
                    matchValue_212_49 = "two-five"
                }
                else -> {
                    matchValue_212_49 = "two"
                }
            }
            matchValue_212_21 = matchValue_212_49
        }
        else -> {
            matchValue_212_21 = "many"
        }
    }
    val text: String = matchValue_212_21
    return offText(text, "two-five")
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
