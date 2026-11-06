func checkFloatRemainder() -> Double {
    let a: Float = 7.5
    let b: Float = 2.0
    return off(a.truncatingRemainder(dividingBy: b), 1.5)
}

func checkFloatRemainderKeepsTheSignOfTheLeftNumber() -> Double {
    let a: Float = Float(0.0 - 7.5)
    let b: Float = 2.0
    return off(a.truncatingRemainder(dividingBy: b), Float(0.0 - 1.5))
}

func checkFloatRemainderIgnoresTheSignOfTheRightNumber() -> Double {
    let a: Float = 7.5
    let b: Float = Float(0.0 - 2.0)
    return off(a.truncatingRemainder(dividingBy: b), 1.5)
}

func checkFloatRemainderOfNumbersThatDivideEvenly() -> Double {
    let b: Float = 47.0
    let c: Float = 97.0
    return off(Float(2867.0.truncatingRemainder(dividingBy: Double(b))), 0.0) + off(Float(5969.0.truncatingRemainder(dividingBy: Double(b))), 0.0) + off(Float(9409.0.truncatingRemainder(dividingBy: Double(c))), 0.0)
}

func checkFloatRemainderOfZero() -> Double {
    let a: Float = 0.0
    let b: Float = 5.0
    return off(a.truncatingRemainder(dividingBy: b), 0.0)
}

func checkFloatRemainderOfAFractionOfOne() -> Double {
    let a: Float = 0.75
    let b: Float = 1.0
    return off(a.truncatingRemainder(dividingBy: b), 0.75)
}

func checkWholeNumberRemainder() -> Double {
    let a: Int64 = 17
    let b: Int64 = 5
    return offInt(a % b, 2) + offInt((0 &- a) % b, 0 &- 2)
}
