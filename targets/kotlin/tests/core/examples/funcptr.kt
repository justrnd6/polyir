fun doubleIt(x: Long): Long {
    return x * 2L
}

fun applyTwice(f: (Long) -> Long, x: Long): Long {
    val once: Long = f(x)
    return f(once)
}

fun applyDoubleTwice(x: Long): Long {
    return applyTwice(::doubleIt, x)
}

fun directDoubleCheck(x: Long): Long {
    return doubleIt(x)
}

fun addOne(x: Long): Long {
    return x + 1L
}

fun pickAndApply(x: Long, useAddOne: Long): Long {
    var fn: (Long) -> Long = ::doubleIt
    if (useAddOne != 0L) {
        fn = ::addOne
    }
    return fn(x)
}

fun applyNTimes(f: (Long) -> Long, x: Long, n: Long): Long {
    var acc: Long = x
    run {
        var i: Long = 0L
        while (i < n) {
            acc = f(acc)
            i = i + 1L
        }
    }
    return acc
}

fun doubleFourTimes(x: Long): Long {
    return applyNTimes(::doubleIt, x, 4L)
}
