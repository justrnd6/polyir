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

// A string's positions count its UTF-8 bytes, as on every target. A byte outside the string is 0, a slice keeps what lies
// inside the string, and a search starts inside it. A char (`Int8`) written is the character of its byte.
fileprivate func textLength(_ text: String) -> Int64 {
    Int64(text.utf8.count)
}

fileprivate func textByte(_ text: String, _ at: Int64) -> Int8 {
    let bytes = Array(text.utf8)
    return at >= 0 && at < Int64(bytes.count) ? Int8(bitPattern: bytes[Int(at)]) : 0
}

fileprivate func textSlice(_ text: String, _ from: Int64, _ to: Int64) -> String {
    let bytes = Array(text.utf8)
    let start = min(max(Int(from), 0), bytes.count)
    let end = min(max(Int(to), start), bytes.count)
    return String(decoding: bytes[start..<end], as: UTF8.self)
}

fileprivate func textFind(_ text: String, _ part: String, _ from: Int64) -> Int64 {
    let bytes = Array(text.utf8)
    let wanted = Array(part.utf8)
    var at = min(max(Int(from), 0), bytes.count)
    while at + wanted.count <= bytes.count {
        if Array(bytes[at..<(at + wanted.count)]) == wanted {
            return Int64(at)
        }
        at += 1
    }
    return -1
}

fileprivate func charText(_ c: Int8) -> String {
    String(UnicodeScalar(UInt8(bitPattern: c)))
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

final class Named: Hashable {
    let name: String
    let tag: String?

    init(_ name: String, _ tag: String?) {
        self.name = name
        self.tag = tag
    }

    static func == (a: Named, b: Named) -> Bool {
        [true, a.name == b.name, a.tag == b.tag].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(name)
        hasher.combine(tag)
    }
}

final class Cell2: Hashable {
    var v: Int64
    var note: String

    init(_ v: Int64, _ note: String) {
        self.v = v
        self.note = note
    }

    func bump() -> Int64 {
        self.v = self.v &+ 1
        return self.v
    }

    func copy() -> Cell2 {
        Cell2(v, note)
    }

    static func == (a: Cell2, b: Cell2) -> Bool {
        [true, a.v == b.v, a.note == b.note].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(v)
        hasher.combine(note)
    }
}

func double(_ x: Int64) -> Int64 {
    return x &* 2
}

func inc(_ x: Int64) -> Int64 {
    return x &+ 1
}

func sets() -> Int64 {
    let seen: MutableSet<Point> = MutableSet()
    seen.items.insert(Point(1, 2))
    seen.items.insert(Point(3, 4))
    seen.items.insert(Point(1, 2))
    var score: Int64 = seen.count
    if seen.items.contains(Point(3, 4)) {
        score = score &+ 10
    }
    let words: MutableSet<String> = MutableSet()
    words.items.insert("a")
    words.items.insert("b")
    words.items.insert("a")
    score = score &+ words.count &* 100
    return score
}

func maps() -> Int64 {
    let m: MutableMap<String, Int64> = MutableMap()
    m["x"] = 5
    m["y"] = 6
    m["x"] = (m["x"] ?? 0) &+ 1
    let byPoint: MutableMap<Point, String> = MutableMap()
    byPoint[Point(1, 1)] = "p"
    var score: Int64 = ((m["x"] ?? 0) &* 10 &+ (m["y"] ?? 0)) &+ m.count
    if byPoint[Point(1, 1)] != nil {
        score = score &+ 100
    }
    if m["none"] ?? 0 == 0 {
        score = score &+ 1000
    }
    return score
}

func strs() -> Int64 {
    let a: String = "ab"
    let b: String = a + "cd" + a
    var score: Int64 = textLength(b)
    if b == "abcdab" {
        score = score &+ 10
    }
    if b != a {
        score = score &+ 100
    }
    let q: String = "say hi {x}"
    score = score &+ textLength(q)
    return score
}

func opts() -> Int64 {
    let n: Named = Named("a", "t")
    let m: Named = Named("a", "t")
    let o: Named = Named("a", nil)
    var score: Int64 = 0
    if n == m {
        score = score &+ 1
    }
    if n == o {
        score = score &+ 10
    }
    if o.tag == nil {
        score = score &+ 100
    }
    return score &+ textLength(n.tag!)
}

func bufs() -> Int64 {
    let b: MutableList<Int64> = MutableList(Array(repeating: 0, count: 4))
    b[0] = 7
    b[3] = 9
    var s: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 4 {
            s = s &+ b[i]
            i = i &+ 1
        }
    }
    return s
}

func swaps() -> Int64 {
    let c: Cell2 = Cell2(1, "n")
    let d: Cell2 = c
    d.bump()
    c.note = c.note + "!"
    if d.note == "n!" {
        return c.v &* 10 &+ textLength(d.note)
    }
    return 0
}

func choose(_ k: Int64) -> Int64 {
    var label: Int64 = 0
    machine: while true {
        switch label {
        case 0:
            if k == 0 {
                label = 1
                continue machine
            } else if k == 1 {
                label = 2
                continue machine
            } else {
                label = 3
                continue machine
            }
        case 1:
            return 100
        case 2:
            return 200
        case 3:
            return 300
        default:
            fatalError("no block \(label)")
        }
    }
}

func loops(_ n: Int64) -> Int64 {
    var s: Int64 = 0
    do {
        var i: Int64 = 0
        var j: Int64 = n
        while i < j {
            s = s &+ i &* j
            i = i &+ 1
            j = j &- 1
        }
    }
    var k: Int64 = 0
    do {
        var t: Int64 = 0
        while t < 4 {
            k = k &+ 1
            t = t &+ 1
        }
    }
    return s &+ k
}

func unsigned() -> Int64 {
    let a: UInt8 = 250
    let b: UInt8 = a &+ 10
    let c: UInt32 = 4000000000
    let d: UInt32 = c &+ c
    let e: Int32 = 2147483647
    let f: Int32 = e &+ 1
    return (Int64(truncatingIfNeeded: b) &+ Int64(truncatingIfNeeded: d)) &+ Int64(truncatingIfNeeded: f) % 1000
}

func floats() -> Int64 {
    let x: Double = 7.5
    let y: Double = 2.25
    let z: Double = x / 2.0 + y * 2.0
    if z > 8.0 && x != y {
        return Int64(z)
    }
    return 0
}

func main() -> Int64 {
    var score: Int64 = 0
    score = (((((score &+ sets()) &+ maps()) &+ strs()) &+ opts()) &+ bufs()) &+ swaps()
    score = (((((score &+ choose(0)) &+ choose(1)) &+ choose(5)) &+ loops(6)) &+ unsigned()) &+ floats()
    return score % 251
}

exit(Int32(truncatingIfNeeded: main()))
