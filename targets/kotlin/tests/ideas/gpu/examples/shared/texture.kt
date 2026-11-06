import kotlin.math.*

private fun floorf(x: Float): Float = floor(x)

fun wrap(n: Int, size: Int, mode: Int): Int {
    if (mode == 0) {
        if (n < 0) {
            return 0
        }
        if (n > size - 1) {
            return size - 1
        }
        return n
    }
    if (mode == 1) {
        return (n % size + size) % size
    }
    val period: Int = size * 2
    val m: Int = (n % period + period) % period
    if (m >= size) {
        return period - 1 - m
    }
    return m
}

fun texel(tex: MutableList<Float>, offset: Int, size: Int, x: Int, y: Int, mode: Int): Float {
    return tex[offset + wrap(y, size, mode) * size + wrap(x, size, mode)]
}

fun nearest(tex: MutableList<Float>, offset: Int, size: Int, u: Float, v: Float, mode: Int): Float {
    val x: Int = floorf(u * (size.toFloat())).toInt()
    val y: Int = floorf(v * (size.toFloat())).toInt()
    return texel(tex, offset, size, x, y, mode)
}

fun lerp(a: Float, b: Float, t: Float): Float {
    return a + (b - a) * t
}

fun bilinear(tex: MutableList<Float>, offset: Int, size: Int, u: Float, v: Float, mode: Int): Float {
    val sx: Float = u * (size.toFloat()) - 0.5f
    val sy: Float = v * (size.toFloat()) - 0.5f
    val x0: Int = floorf(sx).toInt()
    val y0: Int = floorf(sy).toInt()
    val fx: Float = sx - (x0.toFloat())
    val fy: Float = sy - (y0.toFloat())
    val top: Float = lerp(texel(tex, offset, size, x0, y0, mode), texel(tex, offset, size, x0 + 1, y0, mode), fx)
    val bottom: Float = lerp(texel(tex, offset, size, x0, y0 + 1, mode), texel(tex, offset, size, x0 + 1, y0 + 1, mode), fx)
    return lerp(top, bottom, fy)
}

fun levelOffset(base: Int, level: Int): Int {
    var offset: Int = 0
    var size: Int = base
    run {
        var k: Long = 0L
        while (k < level.toLong()) {
            offset = offset + size * size
            size = size / 2
            k = k + 1L
        }
    }
    return offset
}

fun levelSize(base: Int, level: Int): Int {
    var size: Int = base
    run {
        var k: Long = 0L
        while (k < level.toLong()) {
            size = size / 2
            k = k + 1L
        }
    }
    return size
}

fun mipSample(tex: MutableList<Float>, base: Int, levels: Int, u: Float, v: Float, level: Float, mode: Int): Float {
    var lo: Int = floorf(level).toInt()
    if (lo < 0) {
        lo = 0
    }
    if (lo > levels - 1) {
        lo = levels - 1
    }
    var hi: Int = lo + 1
    if (hi > levels - 1) {
        hi = levels - 1
    }
    var amount: Float = level - (lo.toFloat())
    if (amount < 0.0f) {
        amount = 0.0f
    }
    if (amount > 1.0f) {
        amount = 1.0f
    }
    val a: Float = bilinear(tex, levelOffset(base, lo), levelSize(base, lo), u, v, mode)
    val b: Float = bilinear(tex, levelOffset(base, hi), levelSize(base, hi), u, v, mode)
    return lerp(a, b, amount)
}

fun pyramidSize(base: Long): Long {
    var total: Long = 0L
    var size: Long = base
    run {
        var k: Long = 0L
        while (k < 16L) {
            if (size >= 1L) {
                total = total + size * size
                size = size / 2L
            }
            k = k + 1L
        }
    }
    return total
}

fun fillBase(tex: MutableList<Float>, base: Long): Long {
    run {
        var y: Long = 0L
        while (y < base) {
            run {
                var x: Long = 0L
                while (x < base) {
                    var level: Float = if ((x / 4L + y / 4L) % 2L == 1L) 0.45f else 0.85f
                    if (x % 4L == 0L || y % 4L == 0L) {
                        level = level * 0.6f
                    }
                    tex[(y * base + x).toInt()] = level
                    x = x + 1L
                }
            }
            y = y + 1L
        }
    }
    return 0L
}

fun buildPyramid(tex: MutableList<Float>, base: Long): Long {
    fillBase(tex, base)
    var from: Long = 0L
    var size: Long = base
    run {
        var level: Long = 0L
        while (level < 16L) {
            if (size > 1L) {
                shrink(tex, from, size)
                from = from + size * size
                size = size / 2L
            }
            level = level + 1L
        }
    }
    return 0L
}

fun shrink(tex: MutableList<Float>, from: Long, size: Long): Long {
    val next: Long = from + size * size
    val half: Long = size / 2L
    run {
        var y: Long = 0L
        while (y < half) {
            run {
                var x: Long = 0L
                while (x < half) {
                    val a: Float = tex[(from + 2L * y * size + 2L * x).toInt()]
                    val b: Float = tex[(from + 2L * y * size + 2L * x + 1L).toInt()]
                    val c: Float = tex[(from + (2L * y + 1L) * size + 2L * x).toInt()]
                    val d: Float = tex[(from + (2L * y + 1L) * size + 2L * x + 1L).toInt()]
                    tex[(next + y * half + x).toInt()] = (a + b + c + d) * 0.25f
                    x = x + 1L
                }
            }
            y = y + 1L
        }
    }
    return 0L
}
