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

func checkSign() -> Double {
    return off(sign(Float(0.0 - 3.0)), Float(0.0 - 1.0)) + off(sign(0.0), 0.0) + off(sign(2.0), 1.0)
}

func checkFract() -> Double {
    return off(fract(2.75), 0.75) + off(fract(Float(0.0 - 0.25)), 0.75)
}

func checkClamp() -> Double {
    return off(clamp(5.0, Float(0.0 - 1.0), 2.0), 2.0) + off(clamp(Float(0.0 - 5.0), Float(0.0 - 1.0), 2.0), Float(0.0 - 1.0))
}

func checkMix() -> Double {
    return off(mix(10.0, 20.0, 0.25), 12.5)
}

func checkNestedCalls() -> Double {
    return off(clamp(fract(1.75) * 2.0, Float(0.0 - 1.0), 1.0), 1.0) + off(mix(sign(2.0), fract(0.5), 0.5), 0.75)
}

func rescale(_ x: Float) -> Float {
    return x * 3.0
}

func checkOwnFunctionsStillWork() -> Double {
    return off(rescale(fract(1.25)), 0.75)
}
