import Foundation

func sine(_ phase: Float) -> Float {
    return sinf(Float(6.2831853 * Double(fract(phase))))
}

func square(_ phase: Float) -> Float {
    if fract(phase) < 0.5 {
        return 1.0
    }
    return Float(0.0 - 1.0)
}

func sawtooth(_ phase: Float) -> Float {
    return Float(2.0 * Double(fract(phase)) - 1.0)
}

func triangle(_ phase: Float) -> Float {
    return Float(1.0 - 4.0 * Double(fabsf(fract(phase) - 0.5)))
}

func noise(_ seed: Int32) -> Float {
    let a: Int32 = (seed &* 75 &+ 74) % 65537
    let b: Int32 = (a &* 75 &+ 74) % 65537
    return Float(2.0 * Double(Float(b)) / 65537.0 - 1.0)
}

func fade(_ t: Float, _ attack: Float, _ release: Float, _ total: Float) -> Float {
    var level: Float = t < attack ? t / attack : 1.0
    if total - t < release {
        level = level * clamp((total - t) / release, 0.0, 1.0)
    }
    return level
}

func noteFrequency(_ m: Float) -> Float {
    return Float(440.0 * Double(powf(2.0, (m - 69.0) / 12.0)))
}
