// What `throw` throws: the code thrown.
class Thrown(val value: Long) : RuntimeException("thrown $value")

fun riskyDouble(x: Long): Long {
    if (x < 0L) {
        throw Thrown(99L)
    }
    return x * 2L
}

fun safeDouble(x: Long): Long {
    var result: Long = 0L
    try {
        result = riskyDouble(x)
    } catch (thrown: Thrown) {
        val code: Long = thrown.value
        result = code
    }
    return result
}

fun nestedTryCheck(x: Long): Long {
    var result: Long = 0L
    try {
        try {
            result = riskyDouble(x)
        } catch (thrown: Thrown) {
            val innerCode: Long = thrown.value
            result = innerCode + 1L
            throw Thrown(result)
        }
    } catch (thrown: Thrown) {
        val outerCode: Long = thrown.value
        result = outerCode * 10L
    }
    return result
}
