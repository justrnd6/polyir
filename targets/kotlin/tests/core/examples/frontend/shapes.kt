import kotlin.system.exitProcess

data class Pt(val x: Long, val y: Long)
data class Seg(val a: Pt, val b: Pt, val tag: Long)
data class Counter(var hits: Long, val name: String)

fun measure(s: Seg, c: Counter, word: String): Long {
    val dx: Long = s.b.x - s.a.x
    val xs: MutableList<Long> = mutableListOf()
    xs.add(dx)
    xs.add(7L)
    val cells: MutableList<Long> = MutableList(3) { 0L }
    c.hits = c.hits + dx
    return xs.size.toLong() + c.hits
}

fun main(): Long {
    val seg: Seg = Seg(Pt(1L, 2L), Pt(4L, 6L), 9L)
    val c: Counter = Counter(0L, "ctr")
    return measure(seg, c, "hello")
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
