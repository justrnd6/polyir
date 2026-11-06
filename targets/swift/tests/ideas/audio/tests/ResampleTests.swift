import Foundation

func sineSound(_ hz: Float, _ seconds: Float, _ rate: Int64) -> Sound {
    let s: Sound = soundNew(seconds, rate)
    do {
        var i: Int64 = 0
        while i < s.count {
            s.samples[i] = Float(sin(6.283185307179586 * Double(hz) * Double(i) / Double(rate)))
            i = i &+ 1
        }
    }
    return s
}

func checkCountGoingUp() -> Double {
    return offInt(resampledCount(44100, 44100, 48000), 48000)
}

func checkCountGoingDown() -> Double {
    return offInt(resampledCount(1000, 8000, 4000), 500)
}

func checkCountRounds() -> Double {
    return offInt(resampledCount(10, 3, 4), 13)
}

func checkSoundKeepsItsLength() -> Double {
    let s: Sound = sineSound(440.0, 0.5, 8000).resample(12000)
    return offInt(s.rate, 12000) + offInt(s.count, 6000) + offInt(s.channels, 1) + off(s.seconds(), 0.5)
}

func checkUpsamplingFollowsTheWave() -> Double {
    let s: Sound = sineSound(440.0, 1.0, 8000).resample(12000)
    var worst: Double = 0.0
    do {
        var j: Int64 = 1000
        while j < 11000 {
            let expected: Double = sin(6.283185307179586 * 440.0 * Double(j) / 12000.0)
            var gap: Double = Double(s.samples[j]) - expected
            if gap < 0.0 {
                gap = 0.0 - gap
            }
            if gap > worst {
                worst = gap
            }
            j = j &+ 7
        }
    }
    return worst
}

func checkDownsamplingRemovesAliases() -> Double {
    let s: Sound = sineSound(3000.0, 1.0, 8000).resample(4000)
    return Double(amplitudeAt(s.samples, 500, 2000, 4000, 1000.0))
}

func checkDownsamplingKeepsLowTones() -> Double {
    let s: Sound = sineSound(1000.0, 1.0, 8000).resample(4000)
    return off(amplitudeAt(s.samples, 500, 2000, 4000, 1000.0), 1.0)
}

func checkRoundTrip() -> Double {
    let original: Sound = sineSound(300.0, 1.0, 8000)
    let back: Sound = original.resample(12000).resample(8000)
    var worst: Double = 0.0
    do {
        var j: Int64 = 1000
        while j < 7000 {
            var gap: Double = Double(back.samples[j] - original.samples[j])
            if gap < 0.0 {
                gap = 0.0 - gap
            }
            if gap > worst {
                worst = gap
            }
            j = j &+ 11
        }
    }
    return worst
}

func checkConstantStaysConstant() -> Double {
    let s: Sound = soundNew(0.5, 8000)
    do {
        var i: Int64 = 0
        while i < s.count {
            s.samples[i] = 1.0
            i = i &+ 1
        }
    }
    let up: Sound = s.resample(22050)
    let down: Sound = s.resample(3000)
    return off(up.samples[up.count / 2], 1.0) + off(down.samples[down.count / 2], 1.0)
}

func checkStereoResampling() -> Double {
    let left: Sound = sineSound(440.0, 0.5, 8000)
    let right: Sound = sineSound(880.0, 0.5, 8000)
    let out: Sound = soundStereo(left, right).resample(16000)
    return offInt(out.channels, 2) + offInt(out.count, 8000) + off(amplitudeAt(out.channel(0).samples, 2000, 4000, 16000, 440.0), 1.0) + off(amplitudeAt(out.channel(1).samples, 2000, 4000, 16000, 880.0), 1.0)
}

func checkSameRateIsACopy() -> Double {
    let s: Sound = sineSound(440.0, 0.1, 8000)
    let copy: Sound = s.resample(8000)
    var worst: Double = 0.0
    do {
        var i: Int64 = 100
        while i < 700 {
            var gap: Double = Double(copy.samples[i] - s.samples[i])
            if gap < 0.0 {
                gap = 0.0 - gap
            }
            if gap > worst {
                worst = gap
            }
            i = i &+ 1
        }
    }
    return worst + offInt(copy.count, s.count)
}
