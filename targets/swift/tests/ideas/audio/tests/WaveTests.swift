func checkSignOfNegative() -> Double {
    return off(sign(Float(0.0 - 3.0)), Float(0.0 - 1.0))
}

func checkSignOfZero() -> Double {
    return off(sign(0.0), 0.0)
}

func checkSignOfPositive() -> Double {
    return off(sign(2.0), 1.0)
}

func checkFractOfPositive() -> Double {
    return off(fract(2.75), 0.75)
}

func checkFractOfNegative() -> Double {
    return off(fract(Float(0.0 - 0.25)), 0.75)
}

func checkClampAbove() -> Double {
    return off(clamp(5.0, Float(0.0 - 1.0), 2.0), 2.0)
}

func checkClampBelow() -> Double {
    return off(clamp(Float(0.0 - 5.0), Float(0.0 - 1.0), 2.0), Float(0.0 - 1.0))
}

func checkMix() -> Double {
    return off(mix(10.0, 20.0, 0.25), 12.5)
}

func checkSinePeak() -> Double {
    return off(sine(0.25), 1.0)
}

func checkSineTrough() -> Double {
    return off(sine(0.75), Float(0.0 - 1.0))
}

func checkSineZeroCrossing() -> Double {
    return off(sine(1.5), 0.0)
}

func checkSquareHigh() -> Double {
    return off(square(0.25), 1.0)
}

func checkSquareLow() -> Double {
    return off(square(0.75), Float(0.0 - 1.0))
}

func checkSawtoothStart() -> Double {
    return off(sawtooth(0.0), Float(0.0 - 1.0))
}

func checkSawtoothThreeQuarters() -> Double {
    return off(sawtooth(0.75), 0.5)
}

func checkTriangleStart() -> Double {
    return off(triangle(0.0), Float(0.0 - 1.0))
}

func checkTrianglePeak() -> Double {
    return off(triangle(0.5), 1.0)
}

func checkTriangleMiddle() -> Double {
    return off(triangle(0.25), 0.0)
}

func checkConcertA() -> Double {
    return off(noteFrequency(69.0), 440.0)
}

func checkOctaveAboveConcertA() -> Double {
    return off(noteFrequency(81.0), 880.0)
}

func checkFadeInHalfway() -> Double {
    return off(fade(0.5, 1.0, 1.0, 4.0), 0.5)
}

func checkFadeHolds() -> Double {
    return off(fade(2.0, 1.0, 1.0, 4.0), 1.0)
}

func checkFadeOutHalfway() -> Double {
    return off(fade(3.5, 1.0, 1.0, 4.0), 0.5)
}

func checkNoiseRange() -> Double {
    var outside: Int64 = 0
    do {
        var seed: Int64 = 0
        while seed < 1000 {
            if noise(Int32(truncatingIfNeeded: seed)) > 1.0 || noise(Int32(truncatingIfNeeded: seed)) < Float(0.0 - 1.0) {
                outside = outside &+ 1
            }
            seed = seed &+ 1
        }
    }
    return offInt(outside, 0)
}

func checkNoiseRepeats() -> Double {
    return off(noise(42) - noise(42), 0.0)
}

func checkNoiseVaries() -> Double {
    if noise(1) == noise(2) && noise(2) == noise(3) {
        return 1.0
    }
    return 0.0
}
