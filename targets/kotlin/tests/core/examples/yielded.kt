import kotlin.system.exitProcess

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

fun count(n: Long): Coroutine<Long> = Coroutine(iterator {
    var gone: Boolean = false
    run {
        var i: Long = 0L
        while (i < n) {
            if (!gone) {
                gone = run { yield(i); false }
            }
            i = i + 1L
        }
    }
})

fun main(): Long {
    val c: Coroutine<Long> = count(5L)
    val first: Variant = c.resume()
    c.close()
    if (c.done()) {
        return first.value as Long + 40L
    }
    return 1L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
