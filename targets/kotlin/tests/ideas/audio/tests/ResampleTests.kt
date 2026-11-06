import kotlin.math.*

fun sineSound(hz: Float, seconds: Float, rate: Long): Sound {
    val s: Sound = soundNew(seconds, rate)
    run {
        var i: Long = 0L
        while (i < s.count) {
            s.samples[i.toInt()] = sin(6.283185307179586 * (hz.toDouble()) * (i.toDouble()) / (rate.toDouble())).toFloat()
            i = i + 1L
        }
    }
    return s
}

fun checkCountGoingUp(): Double {
    return offInt(resampledCount(44100L, 44100L, 48000L), 48000L)
}

fun checkCountGoingDown(): Double {
    return offInt(resampledCount(1000L, 8000L, 4000L), 500L)
}

fun checkCountRounds(): Double {
    return offInt(resampledCount(10L, 3L, 4L), 13L)
}

fun checkSoundKeepsItsLength(): Double {
    val s: Sound = sineSound(440.0f, 0.5f, 8000L).resample(12000L)
    return offInt(s.rate, 12000L) + offInt(s.count, 6000L) + offInt(s.channels, 1L) + off(s.seconds(), 0.5f)
}

fun checkUpsamplingFollowsTheWave(): Double {
    val s: Sound = sineSound(440.0f, 1.0f, 8000L).resample(12000L)
    var worst: Double = 0.0
    run {
        var j: Long = 1000L
        while (j < 11000L) {
            val expected: Double = sin(6.283185307179586 * 440.0 * (j.toDouble()) / 12000.0)
            var gap: Double = (s.samples[j.toInt()].toDouble()) - expected
            if (gap < 0.0) {
                gap = 0.0 - gap
            }
            if (gap > worst) {
                worst = gap
            }
            j = j + 7L
        }
    }
    return worst
}

fun checkDownsamplingRemovesAliases(): Double {
    val s: Sound = sineSound(3000.0f, 1.0f, 8000L).resample(4000L)
    return amplitudeAt(s.samples, 500L, 2000L, 4000L, 1000.0f).toDouble()
}

fun checkDownsamplingKeepsLowTones(): Double {
    val s: Sound = sineSound(1000.0f, 1.0f, 8000L).resample(4000L)
    return off(amplitudeAt(s.samples, 500L, 2000L, 4000L, 1000.0f), 1.0f)
}

fun checkRoundTrip(): Double {
    val original: Sound = sineSound(300.0f, 1.0f, 8000L)
    val back: Sound = original.resample(12000L).resample(8000L)
    var worst: Double = 0.0
    run {
        var j: Long = 1000L
        while (j < 7000L) {
            var gap: Double = (back.samples[j.toInt()] - original.samples[j.toInt()]).toDouble()
            if (gap < 0.0) {
                gap = 0.0 - gap
            }
            if (gap > worst) {
                worst = gap
            }
            j = j + 11L
        }
    }
    return worst
}

fun checkConstantStaysConstant(): Double {
    val s: Sound = soundNew(0.5f, 8000L)
    run {
        var i: Long = 0L
        while (i < s.count) {
            s.samples[i.toInt()] = 1.0f
            i = i + 1L
        }
    }
    val up: Sound = s.resample(22050L)
    val down: Sound = s.resample(3000L)
    return off(up.samples[(up.count / 2L).toInt()], 1.0f) + off(down.samples[(down.count / 2L).toInt()], 1.0f)
}

fun checkStereoResampling(): Double {
    val left: Sound = sineSound(440.0f, 0.5f, 8000L)
    val right: Sound = sineSound(880.0f, 0.5f, 8000L)
    val out: Sound = soundStereo(left, right).resample(16000L)
    return offInt(out.channels, 2L) + offInt(out.count, 8000L) + off(amplitudeAt(out.channel(0L).samples, 2000L, 4000L, 16000L, 440.0f), 1.0f) + off(amplitudeAt(out.channel(1L).samples, 2000L, 4000L, 16000L, 880.0f), 1.0f)
}

fun checkSameRateIsACopy(): Double {
    val s: Sound = sineSound(440.0f, 0.1f, 8000L)
    val copy: Sound = s.resample(8000L)
    var worst: Double = 0.0
    run {
        var i: Long = 100L
        while (i < 700L) {
            var gap: Double = (copy.samples[i.toInt()] - s.samples[i.toInt()]).toDouble()
            if (gap < 0.0) {
                gap = 0.0 - gap
            }
            if (gap > worst) {
                worst = gap
            }
            i = i + 1L
        }
    }
    return worst + offInt(copy.count, s.count)
}
