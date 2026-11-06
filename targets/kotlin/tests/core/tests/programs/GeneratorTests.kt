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

var made: Long = 0L

fun upTo(limit: Long): Coroutine<Long> = Coroutine(iterator {
    run {
        var i: Long = 0L
        while (i < limit) {
            yield(i)
            i = i + 1L
        }
    }
})

fun counted(limit: Long): Coroutine<Long> {
    made = made + 1L
    return upTo(limit)
}

fun checkSumsEveryValue(): Double {
    var total: Long = 0L
    val forGen_21_10: Coroutine<Long> = upTo(5L)
    var forStep_21_10: Variant = forGen_21_10.resume()
    while (forStep_21_10.tag == 0L) {
        val n: Long = forStep_21_10.value as Long
        total = total + n
        forStep_21_10 = forGen_21_10.resume()
    }
    return offInt(total, 10L)
}

fun checkEmptyCoroutineRunsNoTurns(): Double {
    var turns: Long = 0L
    val forGen_29_10: Coroutine<Long> = upTo(0L)
    var forStep_29_10: Variant = forGen_29_10.resume()
    while (forStep_29_10.tag == 0L) {
        val n: Long = forStep_29_10.value as Long
        turns = turns + 1L
        forStep_29_10 = forGen_29_10.resume()
    }
    return offInt(turns, 0L)
}

fun firstAbove(limit: Long): Long {
    val forGen_36_10: Coroutine<Long> = upTo(100L)
    var forStep_36_10: Variant = forGen_36_10.resume()
    while (forStep_36_10.tag == 0L) {
        val n: Long = forStep_36_10.value as Long
        if (n > limit) {
            forGen_36_10.close()
            return n
        }
        forStep_36_10 = forGen_36_10.resume()
    }
    return -1L
}

fun checkReturnLeavesMidway(): Double {
    return offInt(firstAbove(6L), 7L)
}

fun checkLoopsNest(): Double {
    var total: Long = 0L
    val forGen_50_10: Coroutine<Long> = upTo(3L)
    var forStep_50_10: Variant = forGen_50_10.resume()
    while (forStep_50_10.tag == 0L) {
        val a: Long = forStep_50_10.value as Long
        val forGen_51_14: Coroutine<Long> = upTo(2L)
        var forStep_51_14: Variant = forGen_51_14.resume()
        while (forStep_51_14.tag == 0L) {
            val b: Long = forStep_51_14.value as Long
            total = total + a * 10L + b
            forStep_51_14 = forGen_51_14.resume()
        }
        forStep_50_10 = forGen_50_10.resume()
    }
    return offInt(total, 63L)
}

fun checkSourceIsEvaluatedOnce(): Double {
    made = 0L
    var total: Long = 0L
    val forGen_61_10: Coroutine<Long> = counted(4L)
    var forStep_61_10: Variant = forGen_61_10.resume()
    while (forStep_61_10.tag == 0L) {
        val n: Long = forStep_61_10.value as Long
        total = total + n
        forStep_61_10 = forGen_61_10.resume()
    }
    return offInt(made * 100L + total, 106L)
}
