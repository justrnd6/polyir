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

func scale(_ x: Float, _ k: Float) -> Float {
    return x * k + 1.0
}

func norm(_ a: Float, _ b: Float) -> Float {
    let s: Float = scale(a, 2.0) + b * b
    return sqrtf(s)
}

func sumTo(_ n: Int32) -> Int32 {
    var acc: Int32 = 0
    do {
        var i: Int64 = 0
        while i < Int64(truncatingIfNeeded: n) {
            if i % 2 == 0 {
                acc = acc &+ Int32(truncatingIfNeeded: i)
            }
            i = i &+ 1
        }
    }
    return acc
}

func lane(_ v: [Float]) -> Float {
    return v[Int(0)] + v[Int(3)]
}

func smooth(_ i: Int32, _ src: MutableList<Float>, _ n: Int32) -> Float {
    if i == 0 {
        return src[Int64(i)]
    }
    if i == n &- 1 {
        return src[Int64(i)]
    }
    return (src[Int64(i &- 1)] + src[Int64(i)] + src[Int64(i &+ 1)]) / 3.0
}

func leftover(_ a: Float, _ b: Float) -> Float {
    return a.truncatingRemainder(dividingBy: b)
}

func close(_ a: Float, _ b: Float) -> Int64 {
    var diff: Float = a - b
    if diff < 0.0 {
        diff = Float(0.0 - Double(diff))
    }
    var size: Float = b
    if size < 0.0 {
        size = Float(0.0 - Double(size))
    }
    if diff <= Float(0.000001 * Double(size + 1.0)) {
        return 1
    }
    return 0
}
