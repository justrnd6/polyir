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

func synth(_ i: Int32, _ p: MutableList<Float>) -> Float {
    let t: Float = Float(i) / p[0]
    let envelope: Float = fade(t, p[2], p[3], p[4])
    let wobble: Float = p[6] * sine(p[5] * t)
    var sum: Float = 0.0
    do {
        var k: Int64 = 0
        while k < Int64(truncatingIfNeeded: Int32(p[1])) {
            let cycles: Float = p[8 &+ 2 &* k] * t
            sum = sum + p[9 &+ 2 &* k] * sinf(Float(6.2831853 * Double(cycles - floorf(cycles)) + Double(wobble)))
            k = k &+ 1
        }
    }
    return p[7] * envelope * sum
}

func squareWave(_ i: Int32, _ p: MutableList<Float>) -> Float {
    return sign(sine(p[1] * Float(i) / p[0]))
}

func partialSum(_ i: Int32, _ p: MutableList<Float>) -> Float {
    let x: Float = Float(6.2831853 * Double(Float(i % Int32(p[0]))) / Double(p[0]))
    var sum: Float = 0.0
    do {
        var h: Int64 = 1
        while h <= Int64(truncatingIfNeeded: Int32(p[2])) {
            let number: Float = Float(h)
            let wave: Float = sinf(number * x)
            if p[1] < 0.5 {
                if h % 2 == 1 {
                    sum = sum + wave / number
                }
            }
            if p[1] > 0.5 && p[1] < 1.5 {
                if h % 2 == 1 {
                    sum = sum + wave / number
                }
                if h % 2 == 0 {
                    sum = sum - wave / number
                }
            }
            if p[1] > 1.5 {
                if h % 4 == 1 {
                    sum = sum + wave / (number * number)
                }
                if h % 4 == 3 {
                    sum = sum - wave / (number * number)
                }
            }
            h = h &+ 1
        }
    }
    if p[1] < 0.5 {
        return Float(1.2732395 * Double(sum))
    }
    if p[1] < 1.5 {
        return Float(0.6366198 * Double(sum))
    }
    return Float(0.8105695 * Double(sum))
}

func melody(_ i: Int32, _ notes: MutableList<Float>, _ p: MutableList<Float>) -> Float {
    let t: Float = Float(i) / p[0]
    var sum: Float = 0.0
    do {
        var n: Int64 = 0
        while n < Int64(truncatingIfNeeded: Int32(p[1])) {
            let start: Float = notes[3 &* n]
            let length: Float = notes[3 &* n &+ 1]
            let local: Float = t - start
            if local >= 0.0 && local < length {
                let envelope: Float = fade(local, 0.02, 0.08, length)
                let f: Float = notes[3 &* n &+ 2]
                sum = sum + envelope * (sine(f * t) + Float(0.5 * Double(sine(Float(2.0 * Double(f) * Double(t))))) + Float(0.25 * Double(sine(Float(3.0 * Double(f) * Double(t))))))
            }
            n = n &+ 1
        }
    }
    return p[2] * sum
}
