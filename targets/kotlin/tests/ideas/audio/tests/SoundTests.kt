fun quietTone(): Sound {
    val s: Sound = soundNew(0.1f, 1000L)
    s.tone(0L, 125.0f, 0.5f, 0.0f, 0.1f)
    return s
}

fun checkNewSoundIsMono(): Double {
    return offInt(soundNew(1.0f, 8000L).channels, 1L)
}

fun checkNewSoundLength(): Double {
    return offInt(soundNew(2.5f, 8000L).count, 20000L)
}

fun checkSeconds(): Double {
    return off(soundNew(2.5f, 8000L).seconds(), 2.5f)
}

fun checkTonePeak(): Double {
    return off(quietTone().peak(), 0.5f)
}

fun checkSquareTonePeak(): Double {
    val s: Sound = soundNew(0.1f, 1000L)
    s.tone(1L, 125.0f, 0.4f, 0.0f, 0.1f)
    return off(s.peak(), 0.4f)
}

fun checkScale(): Double {
    val s: Sound = quietTone()
    s.scale(0.5f)
    return off(s.peak(), 0.25f)
}

fun checkMixAdds(): Double {
    val a: Sound = quietTone()
    a.add(quietTone(), 0.0f, 1.0f)
    return off(a.peak(), 1.0f)
}

fun checkNormalize(): Double {
    val s: Sound = soundNew(0.1f, 1000L)
    s.tone(0L, 125.0f, 0.3f, 0.0f, 0.1f)
    s.normalize(0.9f)
    return off(s.peak(), 0.9f)
}

fun checkDelayedMixKeepsLength(): Double {
    val a: Sound = soundNew(1.0f, 1000L)
    a.add(quietTone(), 0.5f, 1.0f)
    return offInt(a.count, 1000L)
}

fun checkDelayedMixIsSilentBefore(): Double {
    val a: Sound = soundNew(1.0f, 1000L)
    a.add(quietTone(), 0.5f, 1.0f)
    return off(a.samples[499], 0.0f)
}

fun checkDelayedMixStartsLater(): Double {
    val a: Sound = soundNew(1.0f, 1000L)
    a.add(quietTone(), 0.5f, 1.0f)
    return off(a.samples[525], 0.35355339f)
}

fun checkToneFadesIn(): Double {
    val s: Sound = soundNew(0.5f, 8000L)
    s.tone(0L, 440.0f, 1.0f, 0.0f, 0.5f)
    return off(s.samples[0], 0.0f)
}

fun checkNoiseAmplitude(): Double {
    val s: Sound = soundNew(0.1f, 8000L)
    s.noise(0.25f, 7L)
    val loudest: Float = s.peak()
    if (loudest > 0.25f) {
        return 1.0
    }
    if (loudest < 0.2f) {
        return 1.0
    }
    return 0.0
}

fun filterGain(kind: Long, hz: Float, cutoff: Float): Float {
    val s: Sound = soundNew(1.0f, 8000L)
    val data: MutableList<Float> = s.samples
    run {
        var i: Long = 0L
        while (i < 8000L) {
            data[i.toInt()] = sine(hz * (i.toFloat()) / 8000.0f)
            i = i + 1L
        }
    }
    if (kind == 0L) {
        s.lowPass(cutoff)
    } else {
        s.highPass(cutoff)
    }
    var loudest: Float = 0.0f
    run {
        var i: Long = 6000L
        while (i < 8000L) {
            var level: Float = data[i.toInt()]
            if (level < 0.0f) {
                level = (0.0 - (level.toDouble())).toFloat()
            }
            if (level > loudest) {
                loudest = level
            }
            i = i + 1L
        }
    }
    return loudest
}

fun checkLowPassCutsHighTone(): Double {
    return off(filterGain(0L, 1000.0f, 500.0f), 0.3987f)
}

fun checkLowPassKeepsLowTone(): Double {
    return off(filterGain(0L, 200.0f, 500.0f), 0.9031f)
}

fun checkHighPassKeepsHighTone(): Double {
    return off(filterGain(1L, 1000.0f, 500.0f), 0.7225f)
}

fun checkHighPassCutsLowTone(): Double {
    return off(filterGain(1L, 200.0f, 500.0f), 0.3613f)
}

fun checkEchoFirstRepeat(): Double {
    val s: Sound = soundNew(0.5f, 1000L)
    s.samples[0] = 1.0f
    s.echo(0.1f, 0.5f)
    return off(s.samples[100], 0.5f)
}

fun checkEchoSecondRepeat(): Double {
    val s: Sound = soundNew(0.5f, 1000L)
    s.samples[0] = 1.0f
    s.echo(0.1f, 0.5f)
    return off(s.samples[200], 0.25f)
}

fun convolved(): Sound {
    val a: Sound = soundNew(0.002f, 1000L)
    val r: Sound = soundNew(0.003f, 1000L)
    a.samples[0] = 1.0f
    a.samples[1] = 2.0f
    r.samples[0] = 1.0f
    r.samples[2] = 3.0f
    return a.convolve(r)
}

fun checkConvolutionLength(): Double {
    return offInt(convolved().count, 4L)
}

fun checkConvolutionValues(): Double {
    val y: MutableList<Float> = convolved().samples
    return off(y[0], 1.0f) + off(y[1], 2.0f) + off(y[2], 3.0f) + off(y[3], 6.0f)
}

fun reverbOfImpulse(): Sound {
    val s: Sound = soundNew(1.0f, 10000L)
    s.samples[0] = 1.0f
    s.reverb(1.0f)
    return s
}

fun checkReverbStart(): Double {
    return off(reverbOfImpulse().samples[0], 0.20525f)
}

fun checkReverbFirstEcho(): Double {
    return off(reverbOfImpulse().samples[297], 0.03924f)
}
