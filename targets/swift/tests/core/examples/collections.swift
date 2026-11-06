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

// A push used as a value gives the list; a list shortened; a list, set or map with one more; two lists joined.
fileprivate func pushed<T>(_ items: MutableList<T>, _ item: T) -> MutableList<T> {
    items.append(item)
    return items
}

fileprivate func truncate<T>(_ items: MutableList<T>, _ count: Int64) -> MutableList<T> {
    if Int(count) < items.items.count {
        items.items.removeLast(items.items.count - max(Int(count), 0))
    }
    return items
}

fileprivate func withItem<T>(_ items: MutableList<T>, _ item: T) -> MutableList<T> {
    MutableList(items.items + [item])
}

fileprivate func withItem<T>(_ items: MutableSet<T>, _ item: T) -> MutableSet<T> {
    MutableSet(items.items.union([item]))
}

fileprivate func withEntry<K, V>(_ table: MutableMap<K, V>, _ key: K, _ value: V) -> MutableMap<K, V> {
    let out = MutableMap(table.items)
    out[key] = value
    return out
}

fileprivate func concat<T>(_ a: MutableList<T>, _ b: MutableList<T>) -> MutableList<T> {
    MutableList(a.items + b.items)
}

final class Point: Hashable {
    let x: Int64
    let y: Int64

    init(_ x: Int64, _ y: Int64) {
        self.x = x
        self.y = y
    }

    static func == (a: Point, b: Point) -> Bool {
        [true, a.x == b.x, a.y == b.y].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(x)
        hasher.combine(y)
    }
}

func grow() -> Int64 {
    let none: MutableList<Int64> = MutableList()
    let one: MutableList<Int64> = withItem(none, 10)
    let two: MutableList<Int64> = withItem(one, 20)
    return ((none.count &* 1000 &+ one.count &* 100) &+ two.count &* 10) &+ two[1] / 10
}

func build() -> MutableList<Int64> {
    let xs: MutableList<Int64> = MutableList()
    do {
        var i: Int64 = 0
        while i < 5 {
            xs.append(i &* i)
            i = i &+ 1
        }
    }
    return MutableList(xs.items)
}

func lookup() -> Int64 {
    let ages: MutableMap<String, Int64> = withEntry(withEntry(MutableMap(), "ann", 31), "bo", 27)
    let seen: MutableSet<Int64> = withItem(withItem(withItem(MutableSet(), 3), 5), 3)
    var score: Int64 = ((ages["ann"] ?? 0) &+ (ages["bo"] ?? 0)) &+ seen.count
    if (ages["cy"] != nil) == false {
        score = score &+ 100
    }
    if seen.items.contains(5) {
        score = score &+ 1000
    }
    return score
}

func visited() -> Int64 {
    let seen: MutableSet<Point> = MutableSet()
    seen.items.insert(Point(1, 2))
    seen.items.insert(Point(3, 4))
    seen.items.insert(Point(1, 2))
    var score: Int64 = seen.count
    if seen.items.contains(Point(3, 4)) {
        score = score &+ 10
    }
    return score
}

func sameThings() -> Int64 {
    let a: MutableMap<Int64, String> = withEntry(withEntry(MutableMap(), 1, "a"), 2, "b")
    let b: MutableMap<Int64, String> = withEntry(withEntry(MutableMap(), 2, "b"), 1, "a")
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a === b {
        score = score &+ 100
    }
    return score
}

func main() -> Int64 {
    return (((grow() &+ build()[4] &* 10000) &+ lookup()) &+ visited()) &+ sameThings()
}

exit(Int32(truncatingIfNeeded: main()))
