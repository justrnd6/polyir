// An `either`: an ok value or an error.
enum Either<T, E> {
    case ok(T)
    case err(E)

    var isOk: Bool {
        if case .ok = self { return true }
        return false
    }

    var okValue: T {
        if case .ok(let value) = self { return value }
        fatalError("not an ok value")
    }

    var errValue: E {
        if case .err(let value) = self { return value }
        fatalError("not an error")
    }
}

extension Either: Equatable where T: Equatable, E: Equatable {}

extension Either: Hashable where T: Hashable, E: Hashable {}

// A variant: the number of its case and the value it holds. Two compare by their cases and what their values say.
struct Variant: Equatable {
    let tag: Int64
    let value: Any?

    static func == (a: Variant, b: Variant) -> Bool {
        a.tag == b.tag && String(describing: a.value) == String(describing: b.value)
    }
}

func safeDivide(_ a: Int64, _ b: Int64) -> Either<Int64, Int64> {
    if b == 0 {
        return Either<Int64, Int64>.err(99)
    }
    return Either<Int64, Int64>.ok(a / b)
}

func divideOrDefault(_ a: Int64, _ b: Int64) -> Int64 {
    let r: Either<Int64, Int64> = safeDivide(a, b)
    if !r.isOk {
        return r.errValue
    }
    return r.okValue
}

func eitherCoercion() -> Int64 {
    let r: Either<Double, Int64> = Either<Double, Int64>.ok(3.0)
    return Int64(r.okValue)
}

func variantSum(_ which: Int64) -> Int64 {
    var v: Variant = Variant(tag: 0, value: Int64(0))
    if which == 0 {
        v = Variant(tag: 0, value: Int64(10))
    }
    if which == 1 {
        v = Variant(tag: 1, value: 2.5)
    }
    if which == 2 {
        v = Variant(tag: 2, value: Int64(30))
    }
    if v.tag == 0 {
        return v.value as! Int64
    }
    if v.tag == 1 {
        return Int64(v.value as! Double) &* 100
    }
    return (v.value as! Int64) &* 1000
}

func castRoundTrip() -> Int64 {
    let d: Double = 9.0
    let asI64: Int64 = Int64(d)
    let backToD: Double = Double(asI64)
    return Int64(backToD) &+ asI64
}
