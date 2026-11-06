func concertAHeight() -> Int64 {
    let quarterCycle: Float = Float(0.25 / Double(noteFrequency(69.0)))
    return Int64(sine(noteFrequency(69.0) * quarterCycle) * 1000.0)
}

func softAttack() -> Int64 {
    return Int64(fade(0.5, 1.0, 1.0, 4.0) * 1000.0)
}

func chordSample(_ t: Float) -> Float {
    return Float(0.3 * Double(sine(noteFrequency(60.0) * t) + sine(noteFrequency(64.0) * t) + sine(noteFrequency(67.0) * t)))
}
