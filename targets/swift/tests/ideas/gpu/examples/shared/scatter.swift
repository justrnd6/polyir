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

func reverseCopy(_ i: Int32, _ src: MutableList<Float>, _ n: Int32, _ dst: MutableList<Float>) -> Int32 {
    dst[Int64((n &- 1) &- i)] = src[Int64(i)]
    return 0
}

func histogram(_ i: Int32, _ data: MutableList<Int32>, _ bins: Int32, _ counts: MutableList<Int32>) -> Int32 {
    let bin: Int32 = data[Int64(i)] % bins
    atomicAdd(counts, bin, 1)
    return 0
}

func histogramRacy(_ i: Int32, _ data: MutableList<Int32>, _ bins: Int32, _ counts: MutableList<Int32>) -> Int32 {
    let bin: Int32 = data[Int64(i)] % bins
    counts[Int64(bin)] = counts[Int64(bin)] &+ 1
    return 0
}

func splat(_ i: Int32, _ p: MutableList<Float>, _ cells: MutableList<Int32>) -> Int32 {
    let w: Int32 = Int32(p[0])
    let arm: Int32 = i % 2
    let t: Float = Float(i / 2 % 20000) / 20000.0
    let angle: Float = t * 9.0 + Float(arm) * 3.14159265
    let radius: Float = Float(0.08 + 0.85 * Double(t))
    let h1: Int32 = (i &* 75 &+ 74) % 65537
    let h2: Int32 = (h1 &* 75 &+ 74) % 65537
    let spread: Float = Float(0.015 + 0.09 * Double(t))
    let x: Float = radius * cosf(angle) + (Float(h1) / 65537.0 - 0.5) * spread * 2.0
    let y: Float = radius * sinf(angle) + (Float(h2) / 65537.0 - 0.5) * spread * 2.0
    let px: Int32 = Int32(floorf((x * 0.5 + 0.5) * Float(w)))
    let py: Int32 = Int32(floorf((y * 0.5 + 0.5) * Float(w)))
    if px >= 0 && px < w && py >= 0 && py < w {
        atomicAdd(cells, py &* w &+ px, 1)
    }
    return 0
}
