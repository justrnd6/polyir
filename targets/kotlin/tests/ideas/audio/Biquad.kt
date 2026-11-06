import kotlin.math.*

private fun pow(x: Double, y: Double): Double = x.pow(y)

data class Biquad(val b0: Double, val b1: Double, val b2: Double, val a1: Double, val a2: Double) {
    fun run(data: MutableList<Float>, count: Long): Long {
        var x1: Double = 0.0
        var x2: Double = 0.0
        var y1: Double = 0.0
        var y2: Double = 0.0
        run {
            var i: Long = 0L
            while (i < count) {
                val x: Double = data[i.toInt()].toDouble()
                val y: Double = this.b0 * x + this.b1 * x1 + this.b2 * x2 - this.a1 * y1 - this.a2 * y2
                x2 = x1
                x1 = x
                y2 = y1
                y1 = y
                data[i.toInt()] = y.toFloat()
                i = i + 1L
            }
        }
        return 0L
    }

    fun gain(hz: Float, rate: Long): Float {
        val w: Double = 6.283185307179586 * (hz.toDouble()) / (rate.toDouble())
        val c1: Double = cos(w)
        val s1: Double = sin(w)
        val c2: Double = cos(2.0 * w)
        val s2: Double = sin(2.0 * w)
        val topReal: Double = this.b0 + this.b1 * c1 + this.b2 * c2
        val topImag: Double = 0.0 - (this.b1 * s1 + this.b2 * s2)
        val bottomReal: Double = 1.0 + this.a1 * c1 + this.a2 * c2
        val bottomImag: Double = 0.0 - (this.a1 * s1 + this.a2 * s2)
        val strength: Double = (topReal * topReal + topImag * topImag) / (bottomReal * bottomReal + bottomImag * bottomImag)
        return sqrt(strength).toFloat()
    }
}

fun biquadFrom(b0: Double, b1: Double, b2: Double, a0: Double, a1: Double, a2: Double): Biquad {
    return Biquad(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0)
}

fun biquadAngle(rate: Long, hz: Float): Double {
    return 6.283185307179586 * (hz.toDouble()) / (rate.toDouble())
}

fun biquadLowPass(rate: Long, hz: Float, q: Float): Biquad {
    val w: Double = biquadAngle(rate, hz)
    val c: Double = cos(w)
    val alpha: Double = sin(w) / (2.0 * (q.toDouble()))
    return biquadFrom((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)
}

fun biquadHighPass(rate: Long, hz: Float, q: Float): Biquad {
    val w: Double = biquadAngle(rate, hz)
    val c: Double = cos(w)
    val alpha: Double = sin(w) / (2.0 * (q.toDouble()))
    return biquadFrom((1.0 + c) / 2.0, 0.0 - (1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)
}

fun biquadBandPass(rate: Long, hz: Float, q: Float): Biquad {
    val w: Double = biquadAngle(rate, hz)
    val c: Double = cos(w)
    val alpha: Double = sin(w) / (2.0 * (q.toDouble()))
    return biquadFrom(alpha, 0.0, 0.0 - alpha, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)
}

fun biquadNotch(rate: Long, hz: Float, q: Float): Biquad {
    val w: Double = biquadAngle(rate, hz)
    val c: Double = cos(w)
    val alpha: Double = sin(w) / (2.0 * (q.toDouble()))
    return biquadFrom(1.0, 0.0 - 2.0 * c, 1.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)
}

fun biquadBell(rate: Long, hz: Float, q: Float, gainDb: Float): Biquad {
    val w: Double = biquadAngle(rate, hz)
    val c: Double = cos(w)
    val alpha: Double = sin(w) / (2.0 * (q.toDouble()))
    val a: Double = pow(10.0, (gainDb.toDouble()) / 40.0)
    return biquadFrom(1.0 + alpha * a, 0.0 - 2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, 0.0 - 2.0 * c, 1.0 - alpha / a)
}

fun biquadLowShelf(rate: Long, hz: Float, gainDb: Float): Biquad {
    val w: Double = biquadAngle(rate, hz)
    val c: Double = cos(w)
    val a: Double = pow(10.0, (gainDb.toDouble()) / 40.0)
    val beta: Double = sqrt(2.0 * a) * sin(w)
    return biquadFrom(a * (a + 1.0 - (a - 1.0) * c + beta), 2.0 * a * (a - 1.0 - (a + 1.0) * c), a * (a + 1.0 - (a - 1.0) * c - beta), a + 1.0 + (a - 1.0) * c + beta, 0.0 - 2.0 * (a - 1.0 + (a + 1.0) * c), a + 1.0 + (a - 1.0) * c - beta)
}

fun biquadHighShelf(rate: Long, hz: Float, gainDb: Float): Biquad {
    val w: Double = biquadAngle(rate, hz)
    val c: Double = cos(w)
    val a: Double = pow(10.0, (gainDb.toDouble()) / 40.0)
    val beta: Double = sqrt(2.0 * a) * sin(w)
    return biquadFrom(a * (a + 1.0 + (a - 1.0) * c + beta), 0.0 - 2.0 * a * (a - 1.0 + (a + 1.0) * c), a * (a + 1.0 + (a - 1.0) * c - beta), a + 1.0 - (a - 1.0) * c + beta, 2.0 * (a - 1.0 - (a + 1.0) * c), a + 1.0 - (a - 1.0) * c - beta)
}
