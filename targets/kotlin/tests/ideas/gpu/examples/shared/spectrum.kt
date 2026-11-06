import kotlin.math.*

private fun sqrtf(x: Float): Float = sqrt(x)
private fun sinf(x: Float): Float = sin(x)
private fun cosf(x: Float): Float = cos(x)

fun dft(k: Int, x: MutableList<Float>, p: MutableList<Float>): Float {
    val n: Int = p[0].toInt()
    var re: Float = 0.0f
    var im: Float = 0.0f
    run {
        var j: Long = 0L
        while (j < n.toLong()) {
            val turns: Float = ((k * (j.toInt()) % n).toFloat()) / (n.toFloat())
            re = re + x[j.toInt()] * cosf((6.2831853 * (turns.toDouble())).toFloat())
            im = im - x[j.toInt()] * sinf((6.2831853 * (turns.toDouble())).toFloat())
            j = j + 1L
        }
    }
    return (2.0 * (sqrtf(re * re + im * im).toDouble()) / ((n.toFloat()).toDouble())).toFloat()
}
