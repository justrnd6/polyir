import kotlin.math.*

private fun floorf(x: Float): Float = floor(x)
private fun sinf(x: Float): Float = sin(x)
private fun cosf(x: Float): Float = cos(x)

fun atomicAdd(counts: MutableList<Int>, slot: Int, amount: Int): Int {
    val old: Int = counts[slot]
    counts[slot] = old + amount
    return old
}

fun reverseCopy(i: Int, src: MutableList<Float>, n: Int, dst: MutableList<Float>): Int {
    dst[n - 1 - i] = src[i]
    return 0
}

fun histogram(i: Int, data: MutableList<Int>, bins: Int, counts: MutableList<Int>): Int {
    val bin: Int = data[i] % bins
    atomicAdd(counts, bin, 1)
    return 0
}

fun histogramRacy(i: Int, data: MutableList<Int>, bins: Int, counts: MutableList<Int>): Int {
    val bin: Int = data[i] % bins
    counts[bin] = counts[bin] + 1
    return 0
}

fun splat(i: Int, p: MutableList<Float>, cells: MutableList<Int>): Int {
    val w: Int = p[0].toInt()
    val arm: Int = i % 2
    val t: Float = ((i / 2 % 20000).toFloat()) / 20000.0f
    val angle: Float = t * 9.0f + (arm.toFloat()) * 3.14159265f
    val radius: Float = (0.08 + 0.85 * (t.toDouble())).toFloat()
    val h1: Int = (i * 75 + 74) % 65537
    val h2: Int = (h1 * 75 + 74) % 65537
    val spread: Float = (0.015 + 0.09 * (t.toDouble())).toFloat()
    val x: Float = radius * cosf(angle) + ((h1.toFloat()) / 65537.0f - 0.5f) * spread * 2.0f
    val y: Float = radius * sinf(angle) + ((h2.toFloat()) / 65537.0f - 0.5f) * spread * 2.0f
    val px: Int = floorf((x * 0.5f + 0.5f) * (w.toFloat())).toInt()
    val py: Int = floorf((y * 0.5f + 0.5f) * (w.toFloat())).toInt()
    if (px >= 0 && px < w && py >= 0 && py < w) {
        atomicAdd(cells, py * w + px, 1)
    }
    return 0
}
