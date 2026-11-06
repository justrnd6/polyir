fun checkSignOfNegative(): Double {
    return off(sign((0.0 - 3.0).toFloat()), (0.0 - 1.0).toFloat())
}

fun checkSignOfZero(): Double {
    return off(sign(0.0f), 0.0f)
}

fun checkSignOfPositive(): Double {
    return off(sign(2.0f), 1.0f)
}

fun checkFractOfPositive(): Double {
    return off(fract(2.75f), 0.75f)
}

fun checkFractOfNegative(): Double {
    return off(fract((0.0 - 0.25).toFloat()), 0.75f)
}

fun checkClampAbove(): Double {
    return off(clamp(5.0f, (0.0 - 1.0).toFloat(), 2.0f), 2.0f)
}

fun checkClampBelow(): Double {
    return off(clamp((0.0 - 5.0).toFloat(), (0.0 - 1.0).toFloat(), 2.0f), (0.0 - 1.0).toFloat())
}

fun checkMix(): Double {
    return off(mix(10.0f, 20.0f, 0.25f), 12.5f)
}

fun checkSinePeak(): Double {
    return off(sine(0.25f), 1.0f)
}

fun checkSineTrough(): Double {
    return off(sine(0.75f), (0.0 - 1.0).toFloat())
}

fun checkSineZeroCrossing(): Double {
    return off(sine(1.5f), 0.0f)
}

fun checkSquareHigh(): Double {
    return off(square(0.25f), 1.0f)
}

fun checkSquareLow(): Double {
    return off(square(0.75f), (0.0 - 1.0).toFloat())
}

fun checkSawtoothStart(): Double {
    return off(sawtooth(0.0f), (0.0 - 1.0).toFloat())
}

fun checkSawtoothThreeQuarters(): Double {
    return off(sawtooth(0.75f), 0.5f)
}

fun checkTriangleStart(): Double {
    return off(triangle(0.0f), (0.0 - 1.0).toFloat())
}

fun checkTrianglePeak(): Double {
    return off(triangle(0.5f), 1.0f)
}

fun checkTriangleMiddle(): Double {
    return off(triangle(0.25f), 0.0f)
}

fun checkConcertA(): Double {
    return off(noteFrequency(69.0f), 440.0f)
}

fun checkOctaveAboveConcertA(): Double {
    return off(noteFrequency(81.0f), 880.0f)
}

fun checkFadeInHalfway(): Double {
    return off(fade(0.5f, 1.0f, 1.0f, 4.0f), 0.5f)
}

fun checkFadeHolds(): Double {
    return off(fade(2.0f, 1.0f, 1.0f, 4.0f), 1.0f)
}

fun checkFadeOutHalfway(): Double {
    return off(fade(3.5f, 1.0f, 1.0f, 4.0f), 0.5f)
}

fun checkNoiseRange(): Double {
    var outside: Long = 0L
    run {
        var seed: Long = 0L
        while (seed < 1000L) {
            if (noise(seed.toInt()) > 1.0f || noise(seed.toInt()) < (0.0 - 1.0).toFloat()) {
                outside = outside + 1L
            }
            seed = seed + 1L
        }
    }
    return offInt(outside, 0L)
}

fun checkNoiseRepeats(): Double {
    return off(noise(42) - noise(42), 0.0f)
}

fun checkNoiseVaries(): Double {
    if (noise(1) == noise(2) && noise(2) == noise(3)) {
        return 1.0
    }
    return 0.0
}
