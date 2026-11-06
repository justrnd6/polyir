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

// A variant: the number of its case and the value it holds.
data class Variant(val tag: Long, val value: Any?)

// A coroutine: the values its body yields, one resume at a time. `resume` gives case 0 with the next value, or case 1 when
// the body has finished. A coroutine closed is not resumed again.
class Coroutine<T>(private val source: Iterator<T>) {
    private var finished = false

    fun resume(): Variant {
        if (finished || !source.hasNext()) {
            finished = true
            return Variant(1L, null)
        }
        return Variant(0L, source.next())
    }

    fun done(): Boolean = finished

    fun close() {
        finished = true
    }
}

// Stops the program with a message, as polyir's `fail` does.
private fun fail(message: String): Nothing {
    System.err.println("trap: " + message)
    exitProcess(1)
}

// What `throw` throws: the code thrown.
class Thrown(val value: Long) : RuntimeException("thrown $value")

var flag: Boolean = false
var ratio: Double = 1.5
var hits: Long = 0L
data class Node(var value: Long, var next: Node?)

data class Holder(val op: (Long) -> Long, val name: String) {
    fun run(x: Long): Long {
        val f: (Long) -> Long = this.op
        return f(x)
    }

    fun viaLater(): (Long) -> Long {
        return fun(y: Long): Long {
            val f: (Long) -> Long = this.op
            return f(y) + textLength(this.name)
        }
    }
}

fun pairs(n: Long): Coroutine<Node> = Coroutine(iterator {
    run {
        var i: Long = 0L
        while (i < n) {
            yield(Node(i, null))
            i = i + 1L
        }
    }
})

fun sumNodes(n: Long): Long {
    val g: Coroutine<Node> = pairs(n)
    var s: Long = 0L
    var done: Boolean = false
    run {
        var k: Long = 0L
        while (k < 20L && done == false) {
            val r: Variant = g.resume()
            if (r.tag == 1L) {
                done = true
            } else {
                s = s + (r.value as Node).value
            }
            k = k + 1L
        }
    }
    return s
}

fun chain(): Long {
    val a: Node = Node(1L, null)
    val b: Node = Node(2L, a)
    val c: Node = Node(3L, b)
    var sum: Long = 0L
    var cur: Node? = c
    run {
        var i: Long = 0L
        while (i < 5L && cur != null) {
            val n: Node = cur ?: fail("unwrap of an empty optional")
            sum = sum + n.value
            cur = n.next
            i = i + 1L
        }
    }
    a.value = 10L
    return sum * 100L + (b.next ?: fail("unwrap of an empty optional")).value
}

fun nestedClosures(a: Long): Long {
    var total: Long = 0L
    val outer: (Long) -> Long = fun(x: Long): Long {
        val inner: (Long) -> Long = fun(y: Long): Long {
            total = total + y
            return x * y + a
        }
        return inner(x + 1L) + inner(2L)
    }
    val r: Long = outer(3L)
    return r * 1000L + total
}

fun strClosure(prefix: String): Long {
    val tail: String = "!"
    val f: (String) -> String = { s -> prefix + s + tail }
    return textLength(f("abc"))
}

fun holders(): Long {
    val h: Holder = Holder({ x -> x * x }, "hh")
    val later: (Long) -> Long = h.viaLater()
    return h.run(5L) + later(2L)
}

fun globals(): Long {
    hits = hits + 5L
    flag = true
    ratio = ratio * 2.0
    var r: Long = hits
    if (flag) {
        r = r + 100L
    }
    if (ratio > 2.5) {
        r = r + 1000L
    }
    return r
}

fun findFirst(limit: Long): Long {
    run {
        var i: Long = 0L
        while (i < limit) {
            run {
                var j: Long = 0L
                while (j < limit) {
                    if (i * j == 12L) {
                        return i * 100L + j
                    }
                    j = j + 1L
                }
            }
            i = i + 1L
        }
    }
    return 0L - 1L
}

fun tryInLoop(n: Long): Long {
    var s: Long = 0L
    run {
        var i: Long = 0L
        while (i < n) {
            try {
                if (i % 2L == 1L) {
                    throw Thrown(i)
                }
                s = s + 10L
            } catch (thrown: Thrown) {
                val code: Long = thrown.value
                s = s + code
            }
            i = i + 1L
        }
    }
    return s
}

fun lines(a: String, b: String): String {
    var label: Long = 0L
    var acc: String = ""
    var n: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                acc = a
                n = 0L
                label = 1L
                continue@machine
            }
            1L -> {
                if (n < 3L) {
                    label = 2L
                    continue@machine
                } else {
                    label = 3L
                    continue@machine
                }
            }
            2L -> {
                acc = acc + b
                n = n + 1L
                label = 1L
                continue@machine
            }
            3L -> {
                return acc
            }
        }
    }
}

fun lineLen(): Long {
    return textLength(lines("x", "yz"))
}

fun machineStrings(n: Long): Long {
    var label: Long = 0L
    var p: Long = 0L
    var s: String = ""
    var q: Long = 0L
    var r: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                if (n == 0L) {
                    p = n
                    s = "p"
                    label = 1L
                    continue@machine
                } else {
                    q = n
                    s = "q"
                    label = 2L
                    continue@machine
                }
            }
            1L -> {
                if (p > 4L) {
                    r = p
                    label = 3L
                    continue@machine
                } else {
                    q = p + 1L
                    s = s + "a"
                    label = 2L
                    continue@machine
                }
            }
            2L -> {
                if (q > 4L) {
                    r = q
                    label = 3L
                    continue@machine
                } else {
                    p = q + 1L
                    s = s + "b"
                    label = 1L
                    continue@machine
                }
            }
            3L -> {
                return r * 100L + textLength(s)
            }
        }
    }
}

fun precedence(a: Long, b: Long): Long {
    val x: Long = (a + b) * (a - b) / 3L
    val y: Long = a and b or (a xor b)
    val z: Long = a shl (2L + b).toInt() shr 1L.toInt()
    var s: Long = x + y + z
    if (a and 1L == 1L && b > 2L || a == 0L) {
        s = s + 1L
    }
    val w: Int = a.toInt()
    if (w.toLong() < 100L && (a + b).toInt() < 1000) {
        s = s + 2L
    }
    return s
}

fun main(): Long {
    var score: Long = sumNodes(5L) + chain() + nestedClosures(2L) + strClosure("ab")
    score = score + holders() + globals() + findFirst(10L) + tryInLoop(6L)
    score = score + lineLen() + machineStrings(0L) + machineStrings(1L) + precedence(13L, 7L)
    return score % 251L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
