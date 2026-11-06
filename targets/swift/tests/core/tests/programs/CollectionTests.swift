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

final class Named: Hashable {
    let name: String
    let at: Point

    init(_ name: String, _ at: Point) {
        self.name = name
        self.at = at
    }

    static func == (a: Named, b: Named) -> Bool {
        [true, a.name == b.name, a.at == b.at].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(name)
        hasher.combine(at)
    }
}

func checkAnArrayGrowsByMakingANewOne() -> Double {
    let a0: MutableList<Int64> = MutableList()
    let a1: MutableList<Int64> = withItem(a0, 10)
    let a2: MutableList<Int64> = withItem(a1, 20)
    return offInt(a0.count, 0) + offInt(a1.count, 1) + offInt(a2.count, 2) + offInt(a2[0], 10) + offInt(a2[1], 20)
}

func checkThawAndFreezeCopy() -> Double {
    let a: MutableList<Int64> = MutableList([1, 2])
    let m: MutableList<Int64> = MutableList(a.items)
    m.append(3)
    let f: MutableList<Int64> = MutableList(m.items)
    m.append(4)
    return offInt(a.count, 2) + offInt(m.count, 4) + offInt(f.count, 3) + offInt(f[2], 3)
}

func checkAMapGrowsByMakingANewOne() -> Double {
    let m0: MutableMap<Int64, Int64> = MutableMap()
    let m1: MutableMap<Int64, Int64> = withEntry(m0, 1, 100)
    var m: MutableMap<Int64, Int64> = m1
    do {
        var i: Int64 = 2
        while i < 30 {
            m = withEntry(m, i, i &* 100)
            i = i &+ 1
        }
    }
    return offInt(m0.count, 0) + offInt(m1.count, 1) + offInt(m.count, 29) + offInt(m[1] ?? 0, 100) + offInt(m[29] ?? 0, 2900) + offInt(m1[2] ?? 0, 0)
}

func checkWithReplacesAnExistingKey() -> Double {
    let m: MutableMap<Int64, Int64> = withEntry(withEntry(MutableMap(), 1, 10), 1, 11)
    return offInt(m.count, 1) + offInt(m[1] ?? 0, 11)
}

func checkHasTellsPresentFromAbsent() -> Double {
    let m: MutableMap<String, Int64> = withEntry(MutableMap(), "a", 0)
    var score: Int64 = 0
    if m["a"] != nil {
        score = score &+ 1
    }
    if (m["b"] != nil) == false {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkASetKeepsEachMemberOnce() -> Double {
    let s: MutableSet<Int64> = withItem(withItem(withItem(MutableSet(), 7), 8), 7)
    var score: Int64 = 0
    if s.items.contains(7) {
        score = score &+ 1
    }
    if s.items.contains(9) == false {
        score = score &+ 2
    }
    return offInt(s.count, 2) + offInt(score, 3)
}

func checkAMutsetChangesInPlace() -> Double {
    let s: MutableSet<String> = MutableSet()
    s.items.insert("x")
    s.items.insert("y")
    s.items.insert("x")
    var score: Int64 = 0
    if s.items.contains("y") {
        score = score &+ 1
    }
    if s.items.contains("z") == false {
        score = score &+ 2
    }
    return offInt(s.count, 2) + offInt(score, 3)
}

func checkAMutmapChangesInPlace() -> Double {
    let m: MutableMap<Int64, Int64> = MutableMap()
    m[1] = 10
    m[2] = 20
    m[1] = 11
    return offInt(m.count, 2) + offInt(m[1] ?? 0, 11) + offInt(m[2] ?? 0, 20)
}

func checkAStructCanBeAKey() -> Double {
    let seen: MutableMap<Point, Int64> = MutableMap()
    seen[Point(1, 2)] = 10
    seen[Point(3, 4)] = 20
    seen[Point(1, 2)] = 11
    var score: Int64 = 0
    if seen[Point(3, 4)] != nil {
        score = score &+ 1
    }
    if (seen[Point(9, 9)] != nil) == false {
        score = score &+ 2
    }
    return offInt(seen.count, 2) + offInt(seen[Point(1, 2)] ?? 0, 11) + offInt(score, 3)
}

func checkAStructWithAStringIsAKeyByContent() -> Double {
    let names: MutableSet<Named> = withItem(withItem(withItem(MutableSet(), Named("a" + "b", Point(1, 1))), Named("ab", Point(1, 1))), Named("c", Point(2, 2)))
    var score: Int64 = 0
    if names.items.contains(Named("c", Point(2, 2))) {
        score = score &+ 1
    }
    return offInt(names.count, 2) + offInt(score, 1)
}

func checkAnImmutableArrayIsAKey() -> Double {
    let lists: MutableMap<MutableList<Int64>, String> = withEntry(withEntry(MutableMap(), MutableList([1, 2]), "one-two"), MutableList([3]), "three")
    var score: Int64 = 0
    if lists[MutableList([1, 2])] ?? "" == "one-two" {
        score = score &+ 1
    }
    if lists[MutableList([3])] ?? "" == "three" {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkMapsAreEqualWhateverTheOrderTheyWereBuiltIn() -> Double {
    let a: MutableMap<Int64, String> = withEntry(withEntry(MutableMap(), 1, "a"), 2, "b")
    let b: MutableMap<Int64, String> = withEntry(withEntry(MutableMap(), 2, "b"), 1, "a")
    let c: MutableMap<Int64, String> = withEntry(withEntry(MutableMap(), 2, "b"), 1, "z")
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a != c {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkSetsAreEqualWhateverTheOrderTheyWereBuiltIn() -> Double {
    let a: MutableSet<String> = withItem(withItem(MutableSet(), "x"), "y")
    let b: MutableSet<String> = withItem(withItem(MutableSet(), "y"), "x")
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a != withItem(MutableSet(), "x") {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkMutableAndImmutableListsAreEqualByContent() -> Double {
    let a: MutableList<Int64> = MutableList([1, 2])
    let b: MutableList<Int64> = MutableList([1, 2])
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a === b {
        score = score &+ 100
    }
    return offInt(score, 1)
}

func checkRepeatedWithInALoopIsNotQuadraticInMemory() -> Double {
    var total: Int64 = 0
    do {
        var r: Int64 = 0
        while r < 200 {
            var a: MutableList<Int64> = MutableList()
            var m: MutableMap<Int64, Int64> = MutableMap()
            var s: MutableSet<Int64> = MutableSet()
            do {
                var i: Int64 = 0
                while i < 20 {
                    a = withItem(a, i)
                    m = withEntry(m, i, i &* 2)
                    s = withItem(s, i % 7)
                    i = i &+ 1
                }
            }
            total = ((total &+ a.count) &+ (m[19] ?? 0)) &+ s.count
            r = r &+ 1
        }
    }
    return offInt(total, 200 &* 65)
}
