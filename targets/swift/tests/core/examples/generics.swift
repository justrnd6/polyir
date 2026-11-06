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

func boxedI64() -> Int64 {
    let b: Box<Int64> = Box(42)
    return b.getValue()
}

func boxedBoth() -> Int64 {
    let bi: Box<Int64> = Box(100)
    let bd: Box<Double> = Box(2.0)
    return bi.getValue() &+ Int64(bd.getValue())
}

func pairOfIntAndDouble() -> Int64 {
    let p: Pair_i64_double = Pair_i64_double(7, 8.5)
    return p.sumAsInt()
}

func doubledI64() -> Int64 {
    let d: Doubler_i64 = Doubler_i64(21)
    return d.doubled()
}

func containerOk() -> Int64 {
    let c: Container<Int64> = Container(Either<Int64, Int64>.ok(42))
    return c.unwrapOrDefault(0)
}

func makePairResult(_ ok: Int64) -> Either<Pair_i64_i64, Int64> {
    if ok == 0 {
        return Either<Pair_i64_i64, Int64>.err(0 &- 1)
    }
    return Either<Pair_i64_i64, Int64>.ok(Pair_i64_i64(11, 22))
}

func pairResultOrErr(_ ok: Int64) -> Int64 {
    let r: Either<Pair_i64_i64, Int64> = makePairResult(ok)
    if !r.isOk {
        return r.errValue
    }
    let p: Pair_i64_i64 = r.okValue
    return p.a &* 100 &+ p.b
}

final class Box<T: Hashable>: Hashable {
    let value: T

    init(_ value: T) {
        self.value = value
    }

    func getValue() -> T {
        return self.value
    }

    static func == (a: Box, b: Box) -> Bool {
        [true, a.value == b.value].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(value)
    }
}

final class Pair_i64_double: Hashable {
    let a: Int64
    let b: Double

    init(_ a: Int64, _ b: Double) {
        self.a = a
        self.b = b
    }

    func sumAsInt() -> Int64 {
        return Int64(truncatingIfNeeded: self.a) &+ Int64(self.b)
    }

    static func == (a: Pair_i64_double, b: Pair_i64_double) -> Bool {
        [true, a.a == b.a, a.b == b.b].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(a)
        hasher.combine(b)
    }
}

final class Doubler_i64: Hashable {
    let value: Int64

    init(_ value: Int64) {
        self.value = value
    }

    func doubled() -> Int64 {
        return self.value &+ self.value
    }

    static func == (a: Doubler_i64, b: Doubler_i64) -> Bool {
        [true, a.value == b.value].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(value)
    }
}

final class Container<T: Hashable>: Hashable {
    let value: Either<T, Int64>

    init(_ value: Either<T, Int64>) {
        self.value = value
    }

    func unwrapOrDefault(_ def: T) -> T {
        if !self.value.isOk {
            return def
        }
        return self.value.okValue
    }

    static func == (a: Container, b: Container) -> Bool {
        [true, a.value == b.value].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(value)
    }
}

final class Pair_i64_i64: Hashable {
    let a: Int64
    let b: Int64

    init(_ a: Int64, _ b: Int64) {
        self.a = a
        self.b = b
    }

    func sumAsInt() -> Int64 {
        return Int64(truncatingIfNeeded: self.a) &+ Int64(truncatingIfNeeded: self.b)
    }

    static func == (a: Pair_i64_i64, b: Pair_i64_i64) -> Bool {
        [true, a.a == b.a, a.b == b.b].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(a)
        hasher.combine(b)
    }
}
