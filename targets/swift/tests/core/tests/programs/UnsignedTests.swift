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

func checkAnUnsignedComparisonDoesNotSeeTheTopBitAsASign() -> Double {
    let big: UInt32 = 4000000000
    var seen: Int64 = 0
    if big > 3000000000 {
        seen = seen &+ 1
    }
    if big >= big {
        seen = seen &+ 10
    }
    if big < 5 {
        seen = seen &+ 100
    }
    return offInt(seen, 11)
}

func checkAnUnsignedDivisionAndRemainder() -> Double {
    let big: UInt32 = 4000000000
    let half: UInt32 = big / 2
    let rest: UInt32 = big % 7
    return offInt(Int64(truncatingIfNeeded: half), 2000000000) + offInt(Int64(truncatingIfNeeded: rest), 3)
}

func checkAnUnsignedRightShiftFillsWithZeros() -> Double {
    let big: UInt32 = 4000000000
    let shifted: UInt32 = big >> 28
    let m: UInt64 = UInt64(truncatingIfNeeded: 0 &- 1)
    return offInt(Int64(truncatingIfNeeded: shifted), 14) + offInt(Int64(truncatingIfNeeded: m >> 63), 1)
}

func checkAnUnsignedWidensWithZeroBits() -> Double {
    let big: UInt32 = 4000000000
    let small: UInt8 = 200
    return offInt(Int64(truncatingIfNeeded: big), 4000000000) + offInt(Int64(truncatingIfNeeded: small), 200)
}

func checkNarrowUnsignedArithmeticWrapsAtItsSize() -> Double {
    let b: UInt8 = 200
    let c: UInt8 = 100
    let sum: UInt8 = b &+ c
    let under: UInt8 = c &- b
    return offInt(Int64(truncatingIfNeeded: sum), 44) + offInt(Int64(truncatingIfNeeded: under), 156)
}

func checkAnUnsignedGoesToAFloatAsItsValue() -> Double {
    let big: UInt32 = 4000000000
    let asDouble: Double = Double(big)
    return offInt(Int64(asDouble), 4000000000)
}

func checkTheLargestU64IsLargerThanFive() -> Double {
    let m: UInt64 = UInt64(truncatingIfNeeded: 0 &- 1)
    var seen: Int64 = 0
    if m > 5 {
        seen = seen &+ 1
    }
    if m / 2 == 9223372036854775807 {
        seen = seen &+ 10
    }
    return offInt(seen, 11)
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

func checkAnUnsignedFieldKeepsItsValueAndStructsCompare() -> Double {
    let a: Pixel = Pixel(250, 128, 7)
    let b: Pixel = Pixel(250, 128, 7)
    var seen: Int64 = 0
    if a == b {
        seen = seen &+ 1
    }
    seen = seen &+ Int64(truncatingIfNeeded: a.g)
    return offInt(seen, 129)
}

func checkUnsignedKeysAreOrdinaryKeys() -> Double {
    let m: MutableMap<UInt32, Int64> = MutableMap()
    let k: UInt32 = 4000000000
    m[k] = 7
    m[5] = 9
    return offInt(m[k] ?? 0, 7) + offInt(m[5] ?? 0, 9)
}
