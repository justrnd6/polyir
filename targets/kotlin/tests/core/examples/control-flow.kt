fun classify(x: Long): Long {
    return if (x < 0L) (0L - 1L) else (if (x == 0L) 0L else 1L)
}

fun maxVal(a: Long, b: Long): Long {
    var result: Long = 0L
    if (a < b) {
        result = b
    } else {
        result = a
    }
    return result
}

fun absViaIf(x: Long): Long {
    var r: Long = x
    if (r < 0L) {
        r = 0L - r
    }
    return r
}
