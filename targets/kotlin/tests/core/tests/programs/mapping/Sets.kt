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

data class Point(val x: Long, val y: Long)
data class Named(val name: String, val tag: String?)

data class Cell2(var v: Long, var note: String) {
    fun bump(): Long {
        this.v = this.v + 1L
        return this.v
    }
}

fun double(x: Long): Long {
    return x * 2L
}

fun inc(x: Long): Long {
    return x + 1L
}

fun sets(): Long {
    val seen: MutableSet<Point> = mutableSetOf()
    seen.add(Point(1L, 2L))
    seen.add(Point(3L, 4L))
    seen.add(Point(1L, 2L))
    var score: Long = seen.size.toLong()
    if (seen.contains(Point(3L, 4L))) {
        score = score + 10L
    }
    val words: MutableSet<String> = mutableSetOf()
    words.add("a")
    words.add("b")
    words.add("a")
    score = score + words.size.toLong() * 100L
    return score
}

fun maps(): Long {
    val m: MutableMap<String, Long> = mutableMapOf()
    m["x"] = 5L
    m["y"] = 6L
    m["x"] = (m["x"] ?: 0L) + 1L
    val byPoint: MutableMap<Point, String> = mutableMapOf()
    byPoint[Point(1L, 1L)] = "p"
    var score: Long = (m["x"] ?: 0L) * 10L + (m["y"] ?: 0L) + m.size.toLong()
    if (byPoint.containsKey(Point(1L, 1L))) {
        score = score + 100L
    }
    if (m["none"] ?: 0L == 0L) {
        score = score + 1000L
    }
    return score
}

fun strs(): Long {
    val a: String = "ab"
    val b: String = a + "cd" + a
    var score: Long = textLength(b)
    if (b == "abcdab") {
        score = score + 10L
    }
    if (b != a) {
        score = score + 100L
    }
    val q: String = "say hi {x}"
    score = score + textLength(q)
    return score
}

fun opts(): Long {
    val n: Named = Named("a", "t")
    val m: Named = Named("a", "t")
    val o: Named = Named("a", null)
    var score: Long = 0L
    if (n == m) {
        score = score + 1L
    }
    if (n == o) {
        score = score + 10L
    }
    if (o.tag == null) {
        score = score + 100L
    }
    return score + textLength(n.tag ?: fail("unwrap of an empty optional"))
}

fun bufs(): Long {
    val b: MutableList<Long> = MutableList(4) { 0L }
    b[0] = 7L
    b[3] = 9L
    var s: Long = 0L
    run {
        var i: Long = 0L
        while (i < 4L) {
            s = s + b[i.toInt()]
            i = i + 1L
        }
    }
    return s
}

fun swaps(): Long {
    val c: Cell2 = Cell2(1L, "n")
    val d: Cell2 = c
    d.bump()
    c.note = c.note + "!"
    if (d.note == "n!") {
        return c.v * 10L + textLength(d.note)
    }
    return 0L
}

fun choose(k: Long): Long {
    var label: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                if (k == 0L) {
                    label = 1L
                    continue@machine
                } else if (k == 1L) {
                    label = 2L
                    continue@machine
                } else {
                    label = 3L
                    continue@machine
                }
            }
            1L -> {
                return 100L
            }
            2L -> {
                return 200L
            }
            3L -> {
                return 300L
            }
        }
    }
}

fun loops(n: Long): Long {
    var s: Long = 0L
    run {
        var i: Long = 0L
        var j: Long = n
        while (i < j) {
            s = s + i * j
            i = i + 1L
            j = j - 1L
        }
    }
    var k: Long = 0L
    run {
        var t: Long = 0L
        while (t < 4L) {
            k = k + 1L
            t = t + 1L
        }
    }
    return s + k
}

fun unsigned(): Long {
    val a: UByte = 250u.toUByte()
    val b: UByte = (a.toUInt() + 10u.toUByte().toUInt()).toUByte()
    val c: UInt = 4000000000u
    val d: UInt = c + c
    val e: Int = 2147483647
    val f: Int = e + 1
    return (b.toLong()) + (d.toLong()) + (f.toLong()) % 1000L
}

fun floats(): Long {
    val x: Double = 7.5
    val y: Double = 2.25
    val z: Double = x / 2.0 + y * 2.0
    if (z > 8.0 && x != y) {
        return z.toLong()
    }
    return 0L
}

fun main(): Long {
    var score: Long = 0L
    score = score + sets() + maps() + strs() + opts() + bufs() + swaps()
    score = score + choose(0L) + choose(1L) + choose(5L) + loops(6L) + unsigned() + floats()
    return score % 251L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
