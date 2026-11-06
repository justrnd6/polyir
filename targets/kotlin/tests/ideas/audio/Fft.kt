import kotlin.math.*

private fun sqrtf(x: Float): Float = sqrt(x)
private fun sinf(x: Float): Float = sin(x)
private fun cosf(x: Float): Float = cos(x)
private fun logf(x: Float): Float = ln(x)

fun fftStage(t: Int, src: MutableList<Float>, p: MutableList<Int>, dst: MutableList<Float>): Int {
    val total: Int = p[0]
    val n: Int = p[1]
    val s: Int = p[2]
    val half: Int = total / 2
    val batch: Int = t / half
    val local: Int = t % half
    val position: Int = local / s
    val column: Int = local % s
    val base: Int = batch * total * 2
    val a: Int = base + ((2L * ((column + s * position).toLong())).toInt())
    val b: Int = base + ((2L * ((column + s * (position + n / 2)).toLong())).toInt())
    val sumRe: Float = src[a] + src[b]
    val sumIm: Float = src[a + 1] + src[b + 1]
    val diffRe: Float = src[a] - src[b]
    val diffIm: Float = src[a + 1] - src[b + 1]
    val angle: Float = ((0.0 - 6.2831853) * ((p[3].toFloat()).toDouble()) * ((position.toFloat()).toDouble()) / ((n.toFloat()).toDouble())).toFloat()
    val twiddleRe: Float = cosf(angle)
    val twiddleIm: Float = sinf(angle)
    val first: Int = base + ((2L * ((column + s * ((2L * (position.toLong())).toInt())).toLong())).toInt())
    val second: Int = base + ((2L * ((column + s * ((2L * (position.toLong()) + 1L).toInt())).toLong())).toInt())
    dst[first] = sumRe
    dst[first + 1] = sumIm
    dst[second] = diffRe * twiddleRe - diffIm * twiddleIm
    dst[second + 1] = diffRe * twiddleIm + diffIm * twiddleRe
    return 0
}

fun fftMagnitude(k: Int, data: MutableList<Float>, p: MutableList<Int>): Float {
    val total: Int = p[0]
    val re: Float = data[(2L * (k.toLong())).toInt()]
    val im: Float = data[(2L * (k.toLong()) + 1L).toInt()]
    return (2.0 * (sqrtf(re * re + im * im).toDouble()) / ((total.toFloat()).toDouble())).toFloat()
}

fun fftFrames(t: Int, signal: MutableList<Float>, p: MutableList<Int>, dst: MutableList<Float>): Int {
    val total: Int = p[0]
    val frame: Int = t / total
    val k: Int = t % total
    val window: Float = (0.5 - 0.5 * (cosf((6.2831853 * ((k.toFloat()).toDouble()) / (((total - 1).toFloat()).toDouble())).toFloat()).toDouble())).toFloat()
    dst[(2L * (t.toLong())).toInt()] = signal[frame * p[1] + k] * window
    dst[(2L * (t.toLong()) + 1L).toInt()] = 0.0f
    return 0
}

fun spectrogramPixel(i: Int, data: MutableList<Float>, p: MutableList<Int>): Float {
    val total: Int = p[0]
    val frames: Int = p[1]
    val rows: Int = p[2]
    val x: Int = i % frames
    val bin: Int = rows - 1 - i / frames
    val at: Int = x * total * 2 + ((2L * (bin.toLong())).toInt())
    val re: Float = data[at]
    val im: Float = data[at + 1]
    val magnitude: Float = (4.0 * (sqrtf(re * re + im * im).toDouble()) / ((total.toFloat()).toDouble())).toFloat()
    var level: Float = ((20.0 * (logf(magnitude + 0.0000001f).toDouble()) / 2.3025851 + 70.0) / 70.0).toFloat()
    if (level < 0.0f) {
        level = 0.0f
    }
    if (level > 1.0f) {
        level = 1.0f
    }
    return level
}

fun complexMultiply(k: Int, a: MutableList<Float>, b: MutableList<Float>, out: MutableList<Float>): Int {
    out[(2L * (k.toLong())).toInt()] = a[(2L * (k.toLong())).toInt()] * b[(2L * (k.toLong())).toInt()] - a[(2L * (k.toLong()) + 1L).toInt()] * b[(2L * (k.toLong()) + 1L).toInt()]
    out[(2L * (k.toLong()) + 1L).toInt()] = a[(2L * (k.toLong())).toInt()] * b[(2L * (k.toLong()) + 1L).toInt()] + a[(2L * (k.toLong()) + 1L).toInt()] * b[(2L * (k.toLong())).toInt()]
    return 0
}

fun realScaled(i: Int, data: MutableList<Float>, p: MutableList<Float>): Float {
    return data[(2L * (i.toLong())).toInt()] / p[0]
}
