import kotlin.system.exitProcess

// What `throw` throws: the code thrown.
class Thrown(val value: Long) : RuntimeException("thrown $value")

data class Pt(val x: Long, val y: Long)
data class Counter(var n: Long, val name: String)

interface Shape {
    fun area(): Long
}

data class Sq(val side: Long) : Shape {
    override fun area(): Long {
        return this.side * this.side
    }
}

data class Rect(val w: Long, val h: Long) : Shape {
    override fun area(): Long {
        return this.w * this.h
    }
}

fun totalArea(shapes: MutableList<Shape>): Long {
    var sum: Long = 0L
    run {
        var i: Long = 0L
        while (i < shapes.size.toLong()) {
            sum = sum + shapes[i.toInt()].area()
            i = i + 1L
        }
    }
    return sum
}

fun risky(x: Long): Long {
    if (x < 0L) {
        throw Thrown(7L)
    }
    return x
}

fun guarded(x: Long): Long {
    var out: Long = 0L
    try {
        out = risky(x)
    } catch (thrown: Thrown) {
        val code: Long = thrown.value
        out = code * 10L
    }
    return out
}

fun main(): Long {
    var score: Long = 0L
    val p: Pt = Pt(3L, 4L)
    val q: Pt = Pt(3L, 4L)
    if (p == q) {
        score = score + 1L
    }
    val c: Counter = Counter(5L, "hits")
    val d: Counter = c
    d.n = d.n + 1L
    if (c === d) {
        score = score + c.n
    }
    val shapes: MutableList<Shape> = mutableListOf()
    shapes.add(Sq(3L))
    shapes.add(Rect(2L, 5L))
    score = score + totalArea(shapes)
    var calls: Long = 0L
    val tick: () -> Long = fun(): Long {
        calls = calls + 1L
        return calls
    }
    val first: Long = tick()
    val second: Long = tick()
    score = score + first + second + calls
    val stock: MutableMap<String, Long> = mutableMapOf()
    stock["apple"] = 3L
    stock["pear"] = 4L
    score = score + (stock["apple"] ?: 0L) * (stock["pear"] ?: 0L)
    val b: Box<Long> = Box(7L)
    score = score + b.value
    score = score + guarded(5L) + guarded(0L - 1L)
    return score
}

data class Box<T>(val value: T)

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
