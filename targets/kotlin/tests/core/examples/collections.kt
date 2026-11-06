import kotlin.system.exitProcess

data class Point(val x: Long, val y: Long)

fun grow(): Long {
    val none: MutableList<Long> = mutableListOf()
    val one: MutableList<Long> = (none + 10L).toMutableList()
    val two: MutableList<Long> = (one + 20L).toMutableList()
    return none.size.toLong() * 1000L + one.size.toLong() * 100L + two.size.toLong() * 10L + two[1] / 10L
}

fun build(): MutableList<Long> {
    val xs: MutableList<Long> = mutableListOf()
    run {
        var i: Long = 0L
        while (i < 5L) {
            xs.add(i * i)
            i = i + 1L
        }
    }
    return xs.toMutableList()
}

fun lookup(): Long {
    val ages: MutableMap<String, Long> = ((mutableMapOf() + ("ann" to 31L)).toMutableMap() + ("bo" to 27L)).toMutableMap()
    val seen: MutableSet<Long> = (((mutableSetOf() + 3L).toMutableSet() + 5L).toMutableSet() + 3L).toMutableSet()
    var score: Long = (ages["ann"] ?: 0L) + (ages["bo"] ?: 0L) + seen.size.toLong()
    if (ages.containsKey("cy") == false) {
        score = score + 100L
    }
    if (seen.contains(5L)) {
        score = score + 1000L
    }
    return score
}

fun visited(): Long {
    val seen: MutableSet<Point> = mutableSetOf()
    seen.add(Point(1L, 2L))
    seen.add(Point(3L, 4L))
    seen.add(Point(1L, 2L))
    var score: Long = seen.size.toLong()
    if (seen.contains(Point(3L, 4L))) {
        score = score + 10L
    }
    return score
}

fun sameThings(): Long {
    val a: MutableMap<Long, String> = ((mutableMapOf() + (1L to "a")).toMutableMap() + (2L to "b")).toMutableMap()
    val b: MutableMap<Long, String> = ((mutableMapOf() + (2L to "b")).toMutableMap() + (1L to "a")).toMutableMap()
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a === b) {
        score = score + 100L
    }
    return score
}

fun main(): Long {
    return grow() + build()[4] * 10000L + lookup() + visited() + sameThings()
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
