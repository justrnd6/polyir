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

final class Contact: Hashable {
    let name: String
    let nickname: String?

    init(_ name: String, _ nickname: String?) {
        self.name = name
        self.nickname = nickname
    }

    static func == (a: Contact, b: Contact) -> Bool {
        [true, a.name == b.name, a.nickname == b.nickname].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(name)
        hasher.combine(nickname)
    }
}

func find(_ xs: MutableList<Int64>, _ want: Int64) -> Int64? {
    do {
        var i: Int64 = 0
        while i < xs.count {
            if xs[i] == want {
                return i
            }
            i = i &+ 1
        }
    }
    return nil
}

func firstPoint(_ ok: Int64) -> (Pt)? {
    if ok == 1 {
        return Pt(7, 8)
    }
    return nil
}

func shown(_ c: Contact) -> String {
    if c.nickname != nil {
        return c.nickname!
    }
    return c.name
}

func main() -> Int64 {
    let xs: MutableList<Int64> = MutableList()
    xs.append(10)
    xs.append(20)
    var score: Int64 = 0
    let hit: Int64? = find(xs, 20)
    if hit != nil {
        score = score &+ hit!
    }
    if find(xs, 99) == nil {
        score = score &+ 10
    }
    let p: (Pt)? = firstPoint(1)
    if p != nil {
        score = score &+ p!.x
    }
    if firstPoint(0) == nil {
        score = score &+ 100
    }
    let named: Contact = Contact("ann", "annie")
    let plain: Contact = Contact("bo", nil)
    if shown(named) == "annie" {
        score = score &+ 1000
    }
    if shown(plain) == "bo" {
        score = score &+ 2000
    }
    return score
}

exit(Int32(truncatingIfNeeded: main()))
