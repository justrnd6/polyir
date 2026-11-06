fun pairSum(i: Int, src: MutableList<Float>, n: Int): Float {
    val a: Int = (2L * (i.toLong())).toInt()
    var total: Float = src[a]
    if (a + 1 < n) {
        total = total + src[a + 1]
    }
    return total
}

fun pairSumInt(i: Int, src: MutableList<Int>, n: Int): Int {
    val a: Int = (2L * (i.toLong())).toInt()
    var total: Int = src[a]
    if (a + 1 < n) {
        total = total + src[a + 1]
    }
    return total
}

fun pairMax(i: Int, src: MutableList<Float>, n: Int): Float {
    val a: Int = (2L * (i.toLong())).toInt()
    var best: Float = src[a]
    if (a + 1 < n) {
        if (src[a + 1] > best) {
            best = src[a + 1]
        }
    }
    return best
}

fun passCount(n: Long): Long {
    var passes: Long = 0L
    var left: Long = n
    run {
        var k: Long = 0L
        while (k < 40L) {
            if (left > 1L) {
                left = (left + 1L) / 2L
                passes = passes + 1L
            }
            k = k + 1L
        }
    }
    return passes
}

fun cpuSum(values: MutableList<Float>, n: Long): Float {
    var total: Float = 0.0f
    run {
        var i: Long = 0L
        while (i < n) {
            total = total + values[i.toInt()]
            i = i + 1L
        }
    }
    return total
}

fun cpuMax(values: MutableList<Float>, n: Long): Float {
    var best: Float = values[0]
    run {
        var i: Long = 1L
        while (i < n) {
            if (values[i.toInt()] > best) {
                best = values[i.toInt()]
            }
            i = i + 1L
        }
    }
    return best
}

fun fillTestData(values: MutableList<Float>, n: Long): Long {
    run {
        var i: Long = 0L
        while (i < n) {
            values[i.toInt()] = (i % 7L).toFloat()
            i = i + 1L
        }
    }
    values[(n / 3L).toInt()] = 1000.0f
    return 0L
}
