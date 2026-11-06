import Foundation

func sign(_ x: Float) -> Float {
    if x > 0.0 {
        return 1.0
    }
    if x < 0.0 {
        return Float(0.0 - 1.0)
    }
    return 0.0
}

func fract(_ x: Float) -> Float {
    return x - floorf(x)
}

func clamp(_ x: Float, _ low: Float, _ high: Float) -> Float {
    if x < low {
        return low
    }
    if x > high {
        return high
    }
    return x
}

func mix(_ a: Float, _ b: Float, _ t: Float) -> Float {
    return a + (b - a) * t
}
