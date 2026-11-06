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

final class Pt: Hashable {
    let x: Int64
    let y: Int64

    init(_ x: Int64, _ y: Int64) {
        self.x = x
        self.y = y
    }

    static func == (a: Pt, b: Pt) -> Bool {
        [true, a.x == b.x, a.y == b.y].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(x)
        hasher.combine(y)
    }
}

final class Seg: Hashable {
    let a: Pt
    let b: Pt
    let tag: Int64

    init(_ a: Pt, _ b: Pt, _ tag: Int64) {
        self.a = a
        self.b = b
        self.tag = tag
    }

    static func == (a: Seg, b: Seg) -> Bool {
        [true, a.a == b.a, a.b == b.b, a.tag == b.tag].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(a)
        hasher.combine(b)
        hasher.combine(tag)
    }
}

final class Counter: Hashable {
    var hits: Int64
    let name: String

    init(_ hits: Int64, _ name: String) {
        self.hits = hits
        self.name = name
    }

    func copy() -> Counter {
        Counter(hits, name)
    }

    static func == (a: Counter, b: Counter) -> Bool {
        [true, a.hits == b.hits, a.name == b.name].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(hits)
        hasher.combine(name)
    }
}

func measure(_ s: Seg, _ c: Counter, _ word: String) -> Int64 {
    let dx: Int64 = s.b.x &- s.a.x
    let xs: MutableList<Int64> = MutableList()
    xs.append(dx)
    xs.append(7)
    let cells: MutableList<Int64> = MutableList(Array(repeating: 0, count: 3))
    c.hits = c.hits &+ dx
    return xs.count &+ c.hits
}

func main() -> Int64 {
    let seg: Seg = Seg(Pt(1, 2), Pt(4, 6), 9)
    let c: Counter = Counter(0, "ctr")
    return measure(seg, c, "hello")
}

exit(Int32(truncatingIfNeeded: main()))
