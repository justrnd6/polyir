import Foundation

// What polyir shares is a class: two names for one list, map or set see each other's changes. Sizes and positions are Int64.
final class MutableList<T>: Sequence {
    var items: [T]

    init(_ items: [T] = []) {
        self.items = items
    }

    var count: Int64 {
        Int64(items.count)
    }

    subscript(at: Int64) -> T {
        get { items[Int(at)] }
        set { items[Int(at)] = newValue }
    }

    func append(_ item: T) {
        items.append(item)
    }

    func makeIterator() -> IndexingIterator<[T]> {
        items.makeIterator()
    }
}

extension MutableList: Equatable where T: Equatable {
    static func == (a: MutableList, b: MutableList) -> Bool {
        a.items == b.items
    }
}

extension MutableList: Hashable where T: Hashable {
    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

final class MutableMap<K: Hashable, V> {
    var items: [K: V]

    init(_ items: [K: V] = [:]) {
        self.items = items
    }

    var count: Int64 {
        Int64(items.count)
    }

    var keys: [K] {
        Array(items.keys)
    }

    subscript(key: K) -> V? {
        get { items[key] }
        set { items[key] = newValue }
    }
}

extension MutableMap: Equatable where V: Equatable {
    static func == (a: MutableMap, b: MutableMap) -> Bool {
        a.items == b.items
    }
}

extension MutableMap: Hashable where V: Hashable {
    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

final class MutableSet<T: Hashable>: Sequence, Hashable {
    var items: Set<T>

    init(_ items: Set<T> = []) {
        self.items = items
    }

    var count: Int64 {
        Int64(items.count)
    }

    func makeIterator() -> Set<T>.Iterator {
        items.makeIterator()
    }

    static func == (a: MutableSet, b: MutableSet) -> Bool {
        a.items == b.items
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

final class Biquad: Hashable {
    let b0: Double
    let b1: Double
    let b2: Double
    let a1: Double
    let a2: Double

    init(_ b0: Double, _ b1: Double, _ b2: Double, _ a1: Double, _ a2: Double) {
        self.b0 = b0
        self.b1 = b1
        self.b2 = b2
        self.a1 = a1
        self.a2 = a2
    }

    func run(_ data: MutableList<Float>, _ count: Int64) -> Int64 {
        var x1: Double = 0.0
        var x2: Double = 0.0
        var y1: Double = 0.0
        var y2: Double = 0.0
        do {
            var i: Int64 = 0
            while i < count {
                let x: Double = Double(data[i])
                let y: Double = self.b0 * x + self.b1 * x1 + self.b2 * x2 - self.a1 * y1 - self.a2 * y2
                x2 = x1
                x1 = x
                y2 = y1
                y1 = y
                data[i] = Float(y)
                i = i &+ 1
            }
        }
        return 0
    }

    func gain(_ hz: Float, _ rate: Int64) -> Float {
        let w: Double = 6.283185307179586 * Double(hz) / Double(rate)
        let c1: Double = cos(w)
        let s1: Double = sin(w)
        let c2: Double = cos(2.0 * w)
        let s2: Double = sin(2.0 * w)
        let topReal: Double = self.b0 + self.b1 * c1 + self.b2 * c2
        let topImag: Double = 0.0 - (self.b1 * s1 + self.b2 * s2)
        let bottomReal: Double = 1.0 + self.a1 * c1 + self.a2 * c2
        let bottomImag: Double = 0.0 - (self.a1 * s1 + self.a2 * s2)
        let strength: Double = (topReal * topReal + topImag * topImag) / (bottomReal * bottomReal + bottomImag * bottomImag)
        return Float(sqrt(strength))
    }

    static func == (a: Biquad, b: Biquad) -> Bool {
        [true, a.b0 == b.b0, a.b1 == b.b1, a.b2 == b.b2, a.a1 == b.a1, a.a2 == b.a2].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(b0)
        hasher.combine(b1)
        hasher.combine(b2)
        hasher.combine(a1)
        hasher.combine(a2)
    }
}

func biquadFrom(_ b0: Double, _ b1: Double, _ b2: Double, _ a0: Double, _ a1: Double, _ a2: Double) -> Biquad {
    return Biquad(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0)
}

func biquadAngle(_ rate: Int64, _ hz: Float) -> Double {
    return 6.283185307179586 * Double(hz) / Double(rate)
}

func biquadLowPass(_ rate: Int64, _ hz: Float, _ q: Float) -> Biquad {
    let w: Double = biquadAngle(rate, hz)
    let c: Double = cos(w)
    let alpha: Double = sin(w) / (2.0 * Double(q))
    return biquadFrom((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)
}

func biquadHighPass(_ rate: Int64, _ hz: Float, _ q: Float) -> Biquad {
    let w: Double = biquadAngle(rate, hz)
    let c: Double = cos(w)
    let alpha: Double = sin(w) / (2.0 * Double(q))
    return biquadFrom((1.0 + c) / 2.0, 0.0 - (1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)
}

func biquadBandPass(_ rate: Int64, _ hz: Float, _ q: Float) -> Biquad {
    let w: Double = biquadAngle(rate, hz)
    let c: Double = cos(w)
    let alpha: Double = sin(w) / (2.0 * Double(q))
    return biquadFrom(alpha, 0.0, 0.0 - alpha, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)
}

func biquadNotch(_ rate: Int64, _ hz: Float, _ q: Float) -> Biquad {
    let w: Double = biquadAngle(rate, hz)
    let c: Double = cos(w)
    let alpha: Double = sin(w) / (2.0 * Double(q))
    return biquadFrom(1.0, 0.0 - 2.0 * c, 1.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)
}

func biquadBell(_ rate: Int64, _ hz: Float, _ q: Float, _ gainDb: Float) -> Biquad {
    let w: Double = biquadAngle(rate, hz)
    let c: Double = cos(w)
    let alpha: Double = sin(w) / (2.0 * Double(q))
    let a: Double = pow(10.0, Double(gainDb) / 40.0)
    return biquadFrom(1.0 + alpha * a, 0.0 - 2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, 0.0 - 2.0 * c, 1.0 - alpha / a)
}

func biquadLowShelf(_ rate: Int64, _ hz: Float, _ gainDb: Float) -> Biquad {
    let w: Double = biquadAngle(rate, hz)
    let c: Double = cos(w)
    let a: Double = pow(10.0, Double(gainDb) / 40.0)
    let beta: Double = sqrt(2.0 * a) * sin(w)
    return biquadFrom(a * (a + 1.0 - (a - 1.0) * c + beta), 2.0 * a * (a - 1.0 - (a + 1.0) * c), a * (a + 1.0 - (a - 1.0) * c - beta), a + 1.0 + (a - 1.0) * c + beta, 0.0 - 2.0 * (a - 1.0 + (a + 1.0) * c), a + 1.0 + (a - 1.0) * c - beta)
}

func biquadHighShelf(_ rate: Int64, _ hz: Float, _ gainDb: Float) -> Biquad {
    let w: Double = biquadAngle(rate, hz)
    let c: Double = cos(w)
    let a: Double = pow(10.0, Double(gainDb) / 40.0)
    let beta: Double = sqrt(2.0 * a) * sin(w)
    return biquadFrom(a * (a + 1.0 + (a - 1.0) * c + beta), 0.0 - 2.0 * a * (a - 1.0 + (a + 1.0) * c), a * (a + 1.0 + (a - 1.0) * c - beta), a + 1.0 - (a - 1.0) * c + beta, 2.0 * (a - 1.0 - (a + 1.0) * c), a + 1.0 - (a - 1.0) * c - beta)
}
