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

// What `throw` throws: the code thrown.
struct Thrown: Error {
    let value: Int64
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

final class Counter: Hashable {
    var n: Int64
    let name: String

    init(_ n: Int64, _ name: String) {
        self.n = n
        self.name = name
    }

    func copy() -> Counter {
        Counter(n, name)
    }

    static func == (a: Counter, b: Counter) -> Bool {
        [true, a.n == b.n, a.name == b.name].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(n)
        hasher.combine(name)
    }
}

protocol Shape: AnyObject {
    func area() -> Int64
}

final class Sq: Shape, Hashable {
    let side: Int64

    init(_ side: Int64) {
        self.side = side
    }

    func area() -> Int64 {
        return self.side &* self.side
    }

    static func == (a: Sq, b: Sq) -> Bool {
        [true, a.side == b.side].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(side)
    }
}

final class Rect: Shape, Hashable {
    let w: Int64
    let h: Int64

    init(_ w: Int64, _ h: Int64) {
        self.w = w
        self.h = h
    }

    func area() -> Int64 {
        return self.w &* self.h
    }

    static func == (a: Rect, b: Rect) -> Bool {
        [true, a.w == b.w, a.h == b.h].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(w)
        hasher.combine(h)
    }
}

func totalArea(_ shapes: MutableList<any Shape>) -> Int64 {
    var sum: Int64 = 0
    do {
        var i: Int64 = 0
        while i < shapes.count {
            sum = sum &+ shapes[i].area()
            i = i &+ 1
        }
    }
    return sum
}

func risky(_ x: Int64) throws -> Int64 {
    if x < 0 {
        throw Thrown(value: 7)
    }
    return x
}

func guarded(_ x: Int64) -> Int64 {
    var out: Int64 = 0
    do {
        out = try risky(x)
    } catch {
        let code = (error as! Thrown).value
        out = code &* 10
    }
    return out
}

func main() -> Int64 {
    var score: Int64 = 0
    let p: Pt = Pt(3, 4)
    let q: Pt = Pt(3, 4)
    if p == q {
        score = score &+ 1
    }
    let c: Counter = Counter(5, "hits")
    let d: Counter = c
    d.n = d.n &+ 1
    if c === d {
        score = score &+ c.n
    }
    let shapes: MutableList<any Shape> = MutableList()
    shapes.append(Sq(3))
    shapes.append(Rect(2, 5))
    score = score &+ totalArea(shapes)
    var calls: Int64 = 0
    let tick: () -> Int64 = { () -> Int64 in
        calls = calls &+ 1
        return calls
    }
    let first: Int64 = tick()
    let second: Int64 = tick()
    score = ((score &+ first) &+ second) &+ calls
    let stock: MutableMap<String, Int64> = MutableMap()
    stock["apple"] = 3
    stock["pear"] = 4
    score = score &+ (stock["apple"] ?? 0) &* (stock["pear"] ?? 0)
    let b: Box<Int64> = Box(7)
    score = score &+ b.value
    score = (score &+ guarded(5)) &+ guarded(0 &- 1)
    return score
}

final class Box<T: Hashable>: Hashable {
    let value: T

    init(_ value: T) {
        self.value = value
    }

    static func == (a: Box, b: Box) -> Bool {
        [true, a.value == b.value].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(value)
    }
}

exit(Int32(truncatingIfNeeded: main()))
