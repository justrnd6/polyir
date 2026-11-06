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

final class Pixel: Hashable {
    let r: UInt8
    let g: UInt8
    let b: UInt8

    init(_ r: UInt8, _ g: UInt8, _ b: UInt8) {
        self.r = r
        self.g = g
        self.b = b
    }

    static func == (a: Pixel, b: Pixel) -> Bool {
        [true, a.r == b.r, a.g == b.g, a.b == b.b].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(r)
        hasher.combine(g)
        hasher.combine(b)
    }
}

func comparison() -> Int64 {
    let big: UInt32 = 4000000000
    if big > 3000000000 && big >= big && (big < 5) == false {
        return 1
    }
    return 0
}

func division() -> Int64 {
    let big: UInt32 = 4000000000
    let half: UInt32 = big / 2
    let rest: UInt32 = big % 7
    if Int64(truncatingIfNeeded: half) == 2000000000 && Int64(truncatingIfNeeded: rest) == 3 {
        return 1
    }
    return 0
}

func shifting() -> Int64 {
    let big: UInt32 = 4000000000
    let top: UInt64 = UInt64(truncatingIfNeeded: 0 &- 1)
    if Int64(truncatingIfNeeded: big >> 28) == 14 && Int64(truncatingIfNeeded: top >> 63) == 1 {
        return 1
    }
    return 0
}

func widening() -> Int64 {
    let big: UInt32 = 4000000000
    let small: UInt8 = 200
    if Int64(truncatingIfNeeded: big) == 4000000000 && Int64(truncatingIfNeeded: small) == 200 {
        return 1
    }
    return 0
}

func wrapping() -> Int64 {
    let a: UInt8 = 200
    let b: UInt8 = 100
    let sum: UInt8 = a &+ b
    let under: UInt8 = b &- a
    let product: UInt16 = 300
    let wide: UInt16 = product &* 300
    if Int64(truncatingIfNeeded: sum) == 44 && Int64(truncatingIfNeeded: under) == 156 && Int64(truncatingIfNeeded: wide) == 24464 {
        return 1
    }
    return 0
}

func floats() -> Int64 {
    let big: UInt32 = 4000000000
    if Int64(Double(big)) == 4000000000 {
        return 1
    }
    return 0
}

func largest() -> Int64 {
    let top: UInt64 = UInt64(truncatingIfNeeded: 0 &- 1)
    if top > 5 && top / 2 == 9223372036854775807 {
        return 1
    }
    return 0
}

func fnv(_ n: Int64) -> UInt32 {
    var h: UInt32 = 2166136261
    do {
        var i: Int64 = 0
        while i < n {
            h = (h ^ UInt32(truncatingIfNeeded: i)) &* 16777619
            i = i &+ 1
        }
    }
    return h
}

func hashing() -> Int64 {
    if Int64(truncatingIfNeeded: fnv(3)) == 581859880 {
        return 1
    }
    return 0
}

func fields() -> Int64 {
    let a: Pixel = Pixel(250, 128, 7)
    let b: Pixel = Pixel(250, 128, 7)
    if a == b && Int64(truncatingIfNeeded: a.g) == 128 && a.r > a.b {
        return 1
    }
    return 0
}

func keys() -> Int64 {
    let m: MutableMap<UInt32, Int64> = MutableMap()
    let k: UInt32 = 4000000000
    m[k] = 7
    m[5] = 9
    if m[k] ?? 0 == 7 && m[5] ?? 0 == 9 {
        return 1
    }
    return 0
}

func main() -> Int64 {
    return ((((((((comparison() &+ division()) &+ shifting()) &+ widening()) &+ wrapping()) &+ floats()) &+ largest()) &+ hashing()) &+ fields()) &+ keys()
}

exit(Int32(truncatingIfNeeded: main()))
