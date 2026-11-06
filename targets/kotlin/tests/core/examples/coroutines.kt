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

private fun putchar(c: Long): Long {
    System.out.write(c.toInt())
    System.out.flush()
    return c
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

fun main(): Long {
    val fib: Coroutine<Long> = fibonacci(7L)
    var sum: Long = 0L
    var done: Boolean = false
    run {
        var i: Long = 0L
        while (i < 8L && done == false) {
            val step: Variant = fib.resume()
            if (step.tag == 1L) {
                done = true
            } else {
                val value: Long = step.value as Long
                putchar(48L + value)
                sum = sum + value
            }
            i = i + 1L
        }
    }
    putchar(10L)
    if (fib.done() == false) {
        return -1L
    }
    return sum
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
