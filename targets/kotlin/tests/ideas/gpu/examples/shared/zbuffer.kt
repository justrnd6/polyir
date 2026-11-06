import kotlin.math.*

private fun fminf(x: Float, y: Float): Float = min(x, y)
private fun fmaxf(x: Float, y: Float): Float = max(x, y)
private fun floorf(x: Float): Float = floor(x)
private fun ceilf(x: Float): Float = ceil(x)

fun atomicMin(values: MutableList<Int>, slot: Int, candidate: Int): Int {
    val old: Int = values[slot]
    if (candidate < old) {
        values[slot] = candidate
    }
    return old
}

fun emptyKey(): Int {
    return 2147483647
}

fun rasterize(t: Int, tris: MutableList<Float>, width: Int, height: Int, depth: MutableList<Int>): Int {
    val base: Int = t * 10
    val x0: Float = tris[base]
    val y0: Float = tris[base + 1]
    val z0: Float = tris[base + 2]
    val x1: Float = tris[base + 3]
    val y1: Float = tris[base + 4]
    val z1: Float = tris[base + 5]
    val x2: Float = tris[base + 6]
    val y2: Float = tris[base + 7]
    val z2: Float = tris[base + 8]
    val area: Float = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    val left: Int = fmaxf(0.0f, floorf(fminf(x0, fminf(x1, x2)))).toInt()
    val right: Int = fminf((width.toFloat()) - 1.0f, ceilf(fmaxf(x0, fmaxf(x1, x2)))).toInt()
    val top: Int = fmaxf(0.0f, floorf(fminf(y0, fminf(y1, y2)))).toInt()
    val bottom: Int = fminf((height.toFloat()) - 1.0f, ceilf(fmaxf(y0, fmaxf(y1, y2)))).toInt()
    run {
        var y: Int = top
        while (y <= bottom) {
            run {
                var x: Int = left
                while (x <= right) {
                    val px: Float = (x.toFloat()) + 0.5f
                    val py: Float = (y.toFloat()) + 0.5f
                    val w0: Float = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
                    val w1: Float = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
                    val w2: Float = (1.0 - (w0.toDouble()) - (w1.toDouble())).toFloat()
                    if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                        val z: Float = w0 * z0 + w1 * z1 + w2 * z2
                        atomicMin(depth, y * width + x, ((z * 1048575.0f).toInt()) * 512 + t)
                    }
                    x = x + 1
                }
            }
            y = y + 1
        }
    }
    return 0
}

fun shade(p: Int, depth: MutableList<Int>, tris: MutableList<Float>): Float {
    val key: Int = depth[p]
    if (key == 2147483647) {
        return 0.06f
    }
    val id: Int = key % 512
    val z: Float = ((key / 512).toFloat()) / 1048575.0f
    return tris[id * 10 + 9] * ((1.0 - 0.45 * (z.toDouble())).toFloat())
}

fun makeScene(tris: MutableList<Float>, count: Long, width: Long, height: Long): Long {
    var seed: Long = 12345L
    run {
        var t: Long = 0L
        while (t < count) {
            val cx: Float = nextRandom(seed) * (width.toFloat())
            seed = nextSeed(seed)
            val cy: Float = nextRandom(seed) * (height.toFloat())
            seed = nextSeed(seed)
            val size: Float = (30.0 + ((nextRandom(seed) * 110.0f).toDouble())).toFloat()
            seed = nextSeed(seed)
            run {
                var corner: Long = 0L
                while (corner < 3L) {
                    tris[(t * 10L + corner * 3L).toInt()] = cx + (nextRandom(seed) - 0.5f) * 2.0f * size
                    seed = nextSeed(seed)
                    tris[(t * 10L + corner * 3L + 1L).toInt()] = cy + (nextRandom(seed) - 0.5f) * 2.0f * size
                    seed = nextSeed(seed)
                    tris[(t * 10L + corner * 3L + 2L).toInt()] = nextRandom(seed)
                    seed = nextSeed(seed)
                    corner = corner + 1L
                }
            }
            tris[(t * 10L + 9L).toInt()] = (0.25 + 0.75 * (nextRandom(seed).toDouble())).toFloat()
            seed = nextSeed(seed)
            t = t + 1L
        }
    }
    return 0L
}

fun nextSeed(seed: Long): Long {
    return (seed * 1103515245L + 12345L) % 2147483648L
}

fun nextRandom(seed: Long): Float {
    return ((seed / 65536L % 32768L).toFloat()) / 32768.0f
}
