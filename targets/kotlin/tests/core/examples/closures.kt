fun counterTwice(): Long {
    var count: Long = 0L
    val inc: () -> Long = fun(): Long {
        count = count + 1L
        return count
    }
    val a: Long = inc()
    val b: Long = inc()
    return a * 100L + b
}

fun makeMultiplier(factor: Long): (Long) -> Long {
    val mul: (Long) -> Long = { x -> x * factor }
    return mul
}

fun tripleOf(x: Long): Long {
    val triple: (Long) -> Long = makeMultiplier(3L)
    return triple(x)
}

fun sharedCounter(): Long {
    var shared: Long = 0L
    val bumpBy1: () -> Long = fun(): Long {
        shared = shared + 1L
        return shared
    }
    val bumpBy10: () -> Long = fun(): Long {
        shared = shared + 10L
        return shared
    }
    val a: Long = bumpBy1()
    val b: Long = bumpBy10()
    val c: Long = bumpBy1()
    return a * 10000L + b * 100L + c
}
