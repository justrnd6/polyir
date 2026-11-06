import kotlin.math.*

private fun fmaxf(x: Float, y: Float): Float = max(x, y)

fun atomicAddFloat(sums: MutableList<Float>, slot: Int, amount: Float): Float {
    val old: Float = sums[slot]
    sums[slot] = old + amount
    return old
}

fun groupSize(): Int {
    return 1
}

fun localIndex(i: Int): Int {
    return 0
}

fun groupIndex(i: Int): Int {
    return i
}

fun barrier(): Int {
    return 0
}

fun blockSum(i: Int, src: MutableList<Float>, n: Int, tile: MutableList<Float>, partial: MutableList<Float>): Int {
    val lane: Int = localIndex(i)
    if (i < n) {
        tile[lane] = src[i]
    } else {
        tile[lane] = 0.0f
    }
    barrier()
    run {
        var s: Int = groupSize() / 2
        while (s > 0) {
            if (lane < s) {
                tile[lane] = tile[lane] + tile[lane + s]
            }
            barrier()
            s = s / 2
        }
    }
    if (lane == 0) {
        partial[groupIndex(i)] = tile[0]
    }
    return 0
}

fun blockMax(i: Int, src: MutableList<Float>, n: Int, tile: MutableList<Float>, partial: MutableList<Float>): Int {
    val lane: Int = localIndex(i)
    if (i < n) {
        tile[lane] = src[i]
    } else {
        tile[lane] = src[0]
    }
    barrier()
    run {
        var s: Int = groupSize() / 2
        while (s > 0) {
            if (lane < s) {
                tile[lane] = fmaxf(tile[lane], tile[lane + s])
            }
            barrier()
            s = s / 2
        }
    }
    if (lane == 0) {
        partial[groupIndex(i)] = tile[0]
    }
    return 0
}

fun total(i: Int, src: MutableList<Float>, n: Int, tile: MutableList<Float>, sum: MutableList<Float>): Int {
    val lane: Int = localIndex(i)
    if (i < n) {
        tile[lane] = src[i]
    } else {
        tile[lane] = 0.0f
    }
    barrier()
    run {
        var s: Int = groupSize() / 2
        while (s > 0) {
            if (lane < s) {
                tile[lane] = tile[lane] + tile[lane + s]
            }
            barrier()
            s = s / 2
        }
    }
    if (lane == 0) {
        atomicAddFloat(sum, 0, tile[0])
    }
    return 0
}

fun groupsFor(n: Long): Long {
    return (n + 255L) / 256L
}

fun launchCount(n: Long): Long {
    var launches: Long = 0L
    var left: Long = n
    run {
        var k: Long = 0L
        while (k < 10L) {
            if (left > 1L) {
                left = groupsFor(left)
                launches = launches + 1L
            }
            k = k + 1L
        }
    }
    return launches
}
