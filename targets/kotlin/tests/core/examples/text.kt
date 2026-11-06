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

// Shortens a list to at most `count` items, and gives it back.
private fun <T> truncate(items: MutableList<T>, count: Long): MutableList<T> {
    while (items.size > maxOf(count, 0L)) {
        items.removeAt(items.size - 1)
    }
    return items
}

fun split(s: String, sep: String): MutableList<String> {
    val parts: MutableList<String> = mutableListOf()
    if (textLength(sep) == 0L) {
        parts.add(s)
        return parts
    }
    var from: Long = 0L
    run {
        var at: Long = textFind(s, sep, 0L)
        while (at >= 0L) {
            parts.add(textSlice(s, from, at))
            from = at + textLength(sep)
            at = textFind(s, sep, from)
        }
    }
    parts.add(textSlice(s, from, textLength(s)))
    return parts
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

fun repeat(s: String, n: Long): String {
    var out: String = ""
    run {
        var i: Long = 0L
        while (i < n) {
            out = out + s
            i = i + 1L
        }
    }
    return out
}

fun fill(template: String, values: MutableMap<String, String>): String {
    var out: String = ""
    var from: Long = 0L
    run {
        var at: Long = textFind(template, "\${", 0L)
        while (at >= 0L) {
            val close: Long = textFind(template, "}", at)
            if (close < 0L) {
                return out + textSlice(template, from, textLength(template))
            }
            out = out + textSlice(template, from, at) + filledName(textSlice(template, at, close + 1L), values)
            from = close + 1L
            at = textFind(template, "\${", from)
        }
    }
    return out + textSlice(template, from, textLength(template))
}

fun filledName(placeholder: String, values: MutableMap<String, String>): String {
    val name: String = trim(textSlice(placeholder, 2L, textLength(placeholder) - 1L))
    if (values.containsKey(name)) {
        return values[name] ?: ""
    }
    return placeholder
}

data class StringBuilder(val pieces: MutableList<String>, val indents: MutableList<Long>) {
    fun add(s: String): StringBuilder {
        this.pieces.add(s)
        return this
    }

    fun line(s: String): StringBuilder {
        if (textLength(s) > 0L) {
            this.pieces.add(repeat(" ", this.indentNow()))
        }
        this.pieces.add(s)
        this.pieces.add("\n")
        return this
    }

    fun blank(): StringBuilder {
        this.pieces.add("\n")
        return this
    }

    fun lines(all: MutableList<String>): StringBuilder {
        run {
            var i: Long = 0L
            while (i < all.size.toLong()) {
                this.line(all[i.toInt()])
                i = i + 1L
            }
        }
        return this
    }

    fun indent(n: Long): StringBuilder {
        this.indents.add(this.indentNow() + n)
        return this
    }

    fun dedent(): StringBuilder {
        if (this.indents.size.toLong() > 0L) {
            truncate(this.indents, this.indents.size.toLong() - 1L)
        }
        return this
    }

    fun indentNow(): Long {
        if (this.indents.size.toLong() == 0L) {
            return 0L
        }
        return this.indents[(this.indents.size.toLong() - 1L).toInt()]
    }

    fun length(): Long {
        return textLength(this.text())
    }

    fun isEmpty(): Boolean {
        return this.length() == 0L
    }

    fun text(): String {
        return join(this.pieces, "")
    }
}

data class Dep(val name: String, val version: String)

fun cmakeLists(name: String, sources: MutableList<String>, deps: MutableList<Dep>): String {
    val out: StringBuilder = StringBuilder(mutableListOf(), mutableListOf())
    for (d in deps) {
        out.line("pkg_check_modules(${d.name} REQUIRED IMPORTED_TARGET ${d.name}>=${d.version})")
    }
    return "cmake_minimum_required(VERSION 3.20)" + "\n" + "project(" + name + " LANGUAGES C)" + "\n" + "find_package(PkgConfig REQUIRED)" + "\n" + trim(out.text()) + "\n" + "add_executable(" + name + " " + join(sources, " ") + ")" + "\n"
}

fun checksum(s: String): Long {
    var sum: Long = 0L
    run {
        var i: Long = 0L
        while (i < textLength(s)) {
            sum = (sum * 31L + (textByte(s, i).toLong())) % 1000003L
            i = i + 1L
        }
    }
    return sum
}

fun main(): Long {
    val deps: MutableList<Dep> = mutableListOf(Dep("zlib", "1.2"), Dep("sdl2", "2.0"))
    val text: String = cmakeLists("demo", mutableListOf("main.c", "util.c"), deps)
    val values: MutableMap<String, String> = mutableMapOf()
    values["who"] = "polyir"
    val hello: String = fill("hello \${who}", values)
    return (checksum(text) + split("a, b ,c", ",").size.toLong() * 7L + textLength(hello)) % 256L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
