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

fun fibonacci(count: Long): Coroutine<Long> = Coroutine(iterator {
    var a: Long = 0L
    var b: Long = 1L
    run {
        var i: Long = 0L
        while (i < count) {
            yield(a)
            val next: Long = a + b
            a = b
            b = next
            i = i + 1L
        }
    }
})

fun countdown(from: Long): Coroutine<Long> = Coroutine(iterator {
    var n: Long = from
    run {
        var i: Long = 0L
        while (i < from) {
            yield(n)
            n = n - 1L
            i = i + 1L
        }
    }
})

fun total(source: Coroutine<Long>): Long {
    var sum: Long = 0L
    var finished: Boolean = false
    run {
        var i: Long = 0L
        while (i < 100L && finished == false) {
            val step: Variant = source.resume()
            if (step.tag == 1L) {
                finished = true
            } else {
                sum = sum + step.value as Long
            }
            i = i + 1L
        }
    }
    return sum
}

fun main(): Long {
    var score: Long = total(fibonacci(7L))
    score = score + total(countdown(4L))
    val two: Coroutine<Long> = countdown(3L)
    val first: Variant = two.resume()
    val second: Variant = two.resume()
    score = score + first.value as Long * 10L + second.value as Long
    if (two.done() == false) {
        score = score + 100L
    }
    return score
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
