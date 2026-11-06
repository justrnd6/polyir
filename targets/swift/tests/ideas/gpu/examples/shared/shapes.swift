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

func atomicAdd(_ counts: MutableList<Int32>, _ slot: Int32, _ amount: Int32) -> Int32 {
    let old: Int32 = counts[Int64(slot)]
    counts[Int64(slot)] = old &+ amount
    return old
}

func atomicAddFloat(_ sums: MutableList<Float>, _ slot: Int32, _ amount: Float) -> Float {
    let old: Float = sums[Int64(slot)]
    sums[Int64(slot)] = old + amount
    return old
}

func atomicMin(_ values: MutableList<Int32>, _ slot: Int32, _ candidate: Int32) -> Int32 {
    let old: Int32 = values[Int64(slot)]
    if candidate < old {
        values[Int64(slot)] = candidate
    }
    return old
}

func atomicMax(_ values: MutableList<Int32>, _ slot: Int32, _ candidate: Int32) -> Int32 {
    let old: Int32 = values[Int64(slot)]
    if candidate > old {
        values[Int64(slot)] = candidate
    }
    return old
}

func blend7(_ a: Float, _ b: Float, _ c: Float, _ d: Float, _ e: Float, _ f: Float, _ g: Float, _ k: Float) -> Float {
    return k * (a + Float(2.0 * Double(b)) + Float(3.0 * Double(c))) + (d - e) * (f + g)
}

func plasma(_ x: Int32, _ y: Int32, _ width: Int32, _ height: Int32, _ phase: Float) -> Float {
    let u: Float = Float(x) / Float(width)
    let v: Float = Float(y) / Float(height)
    let a: Float = sinf(Float(10.0 * Double(u) + Double(phase)))
    let b: Float = sinf(Float(8.0 * Double(v) - Double(phase)))
    let c: Float = sinf(Float(6.0 * Double(u + v) + 2.0 * Double(phase)))
    return Float(0.5 + 0.1667 * Double(a + b + c))
}

func vignette(_ pixel: Float, _ x: Int32, _ y: Int32, _ width: Int32, _ height: Int32) -> Float {
    let dx: Float = Float(x) / Float(width) - 0.5
    let dy: Float = Float(y) / Float(height) - 0.5
    let edge: Float = Float(1.0 - 1.6 * Double(dx * dx + dy * dy))
    return pixel * edge
}

func tally(_ i: Int32, _ counts: MutableList<Int32>, _ sums: MutableList<Float>, _ lows: MutableList<Int32>, _ highs: MutableList<Int32>) -> Int32 {
    let value: Int32 = i &* 7919 % 10007
    atomicAdd(counts, i % 4, 1)
    atomicAddFloat(sums, i % 4, 0.5)
    atomicMin(lows, 0, value)
    atomicMax(highs, 0, value)
    return 0
}

func reported(_ i: Int32) -> Int32 {
    return i &* 7919 % 10007
}

func nearly(_ a: Float, _ b: Float, _ tolerance: Float) -> Int64 {
    var gap: Float = a - b
    if gap < 0.0 {
        gap = Float(0.0 - Double(gap))
    }
    if gap <= tolerance {
        return 1
    }
    return 0
}
