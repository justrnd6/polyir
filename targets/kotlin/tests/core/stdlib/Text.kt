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

fun lines(s: String): MutableList<String> {
    return split(s, "\n")
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

fun replace(s: String, old: String, by: String): String {
    return join(split(s, old), by)
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

fun indent(s: String, n: Long): String {
    val all: MutableList<String> = lines(s)
    val pad: String = repeat(" ", n)
    run {
        var i: Long = 0L
        while (i < all.size.toLong()) {
            if (textLength(all[i.toInt()]) > 0L) {
                all[i.toInt()] = pad + all[i.toInt()]
            }
            i = i + 1L
        }
    }
    return join(all, "\n")
}

fun indentRest(s: String, n: Long): String {
    val first: Long = textFind(s, "\n", 0L)
    if (first < 0L) {
        return s
    }
    return textSlice(s, 0L, first + 1L) + indent(textSlice(s, first + 1L, textLength(s)), n)
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
