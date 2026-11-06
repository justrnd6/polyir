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

fun evens(limit: Long): Coroutine<Long> = Coroutine(iterator {
    run {
        var i: Long = 0L
        while (i < limit) {
            yield(i)
            i = i + 2L
        }
    }
})

fun sumOfEvens(): Long {
    var total: Long = 0L
    val forGen_14_8: Coroutine<Long> = evens(10L)
    var forStep_14_8: Variant = forGen_14_8.resume()
    while (forStep_14_8.tag == 0L) {
        val n: Long = forStep_14_8.value as Long
        total = total + n
        forStep_14_8 = forGen_14_8.resume()
    }
    return total
}

fun firstOver(limit: Long): Long {
    val forGen_22_8: Coroutine<Long> = evens(100L)
    var forStep_22_8: Variant = forGen_22_8.resume()
    while (forStep_22_8.tag == 0L) {
        val n: Long = forStep_22_8.value as Long
        if (n > limit) {
            forGen_22_8.close()
            return n
        }
        forStep_22_8 = forGen_22_8.resume()
    }
    return -1L
}

fun pairs(): Long {
    var total: Long = 0L
    val forGen_33_8: Coroutine<Long> = evens(6L)
    var forStep_33_8: Variant = forGen_33_8.resume()
    while (forStep_33_8.tag == 0L) {
        val a: Long = forStep_33_8.value as Long
        val forGen_34_10: Coroutine<Long> = evens(4L)
        var forStep_34_10: Variant = forGen_34_10.resume()
        while (forStep_34_10.tag == 0L) {
            val b: Long = forStep_34_10.value as Long
            total = total + a * 10L + b
            forStep_34_10 = forGen_34_10.resume()
        }
        forStep_33_8 = forGen_33_8.resume()
    }
    return total
}

fun main(): Long {
    return sumOfEvens() + firstOver(5L) + pairs()
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
