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

// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero. `set(value, row, col)` writes one.
final class Sparse<T> {
    private var rows: [Int64] = []
    private var cols: [Int64] = []
    private var values: [T] = []
    private let zero: T

    init(_ zero: T) {
        self.zero = zero
    }

    var count: Int64 {
        Int64(values.count)
    }

    func get(_ row: Int64, _ col: Int64) -> T {
        var found = zero
        for i in values.indices where rows[i] == row && cols[i] == col {
            found = values[i]
        }
        return found
    }

    func set(_ value: T, _ row: Int64, _ col: Int64) {
        rows.append(row)
        cols.append(col)
        values.append(value)
    }

    func copy() -> Sparse<T> {
        let out = Sparse(zero)
        out.rows = rows
        out.cols = cols
        out.values = values
        return out
    }
}

final class Counter: Hashable {
    var n: Int64
    let id: Int64

    init(_ n: Int64, _ id: Int64) {
        self.n = n
        self.id = id
    }

    func bump() -> Int64 {
        self.n = self.n &+ 1
        return self.n
    }

    func copy() -> Counter {
        Counter(n, id)
    }

    static func == (a: Counter, b: Counter) -> Bool {
        [true, a.n == b.n, a.id == b.id].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(n)
        hasher.combine(id)
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

final class Holder: Hashable {
    var items: MutableList<Int64>
    var inner: Counter

    init(_ items: MutableList<Int64>, _ inner: Counter) {
        self.items = items
        self.inner = inner
    }

    func copy() -> Holder {
        Holder(items, inner)
    }

    static func == (a: Holder, b: Holder) -> Bool {
        [true, a.items == b.items, a.inner == b.inner].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
        hasher.combine(inner)
    }
}

protocol Reading: AnyObject {
    func read() -> Int64
}

final class Gauge: Reading, Hashable {
    var level: Int64

    init(_ level: Int64) {
        self.level = level
    }

    func read() -> Int64 {
        return self.level
    }

    func copy() -> Gauge {
        Gauge(level)
    }

    static func == (a: Gauge, b: Gauge) -> Bool {
        [true, a.level == b.level].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(level)
    }
}

func addTen(_ c: Counter) -> Int64 {
    c.n = c.n &+ 10
    return 0
}

func addTenOnACopy(_ c: Counter) -> Int64 {
    let c: Counter = c.copy()
    c.n = c.n &+ 10
    return c.n
}

func checkTwoNamesForOneObjectShareIt() -> Double {
    let a: Counter = Counter(1, 7)
    let b: Counter = a
    b.n = 5
    return offInt(a.n, 5) + offInt(a.id, 7)
}

func checkACalleeChangesTheCallersObject() -> Double {
    let a: Counter = Counter(1, 0)
    addTen(a)
    return offInt(a.n, 11)
}

func checkAValParameterIsTheCalleesOwnCopy() -> Double {
    let a: Counter = Counter(1, 0)
    let seen: Int64 = addTenOnACopy(a)
    return offInt(seen, 11) + offInt(a.n, 1)
}

func checkValInFrontOfAnExpressionCopies() -> Double {
    let a: Counter = Counter(1, 3)
    let b: Counter = a.copy()
    b.n = 50
    return offInt(a.n, 1) + offInt(b.n, 50) + offInt(b.id, 3)
}

func checkCopyMethodAndValAgree() -> Double {
    let a: Counter = Counter(4, 0)
    let b: Counter = a.copy()
    a.n = 9
    return offInt(b.n, 4)
}

func checkAMethodChangesThis() -> Double {
    let a: Counter = Counter(0, 0)
    a.bump()
    a.bump()
    return offInt(a.n, 2)
}

func checkAnImmutableStructIsAValue() -> Double {
    let p: Pt = Pt(1, 2)
    let q: Pt = p
    let r: Pt = p
    return offInt(q.x &+ r.y, 3)
}

func checkAFieldHoldingAnotherReferenceStruct() -> Double {
    let h: Holder = Holder(MutableList(), Counter(1, 0))
    let alias: Holder = h
    alias.inner.n = 8
    alias.items.append(5)
    return offInt(h.inner.n, 8) + offInt(h.items.count, 1)
}

func checkAFieldCanBeReplacedWithAnotherObject() -> Double {
    let h: Holder = Holder(MutableList(), Counter(1, 0))
    let keep: Counter = h.inner
    h.inner = Counter(2, 0)
    return offInt(keep.n, 1) + offInt(h.inner.n, 2)
}

func checkAGenericStructWithAVarField() -> Double {
    let c: Cell<Int64> = Cell(1)
    let d: Cell<Int64> = c
    d.v = 9
    return offInt(c.v, 9)
}

func checkAnInterfaceValueSeesLaterChanges() -> Double {
    let g: Gauge = Gauge(1)
    let r: any Reading = g
    g.level = 42
    return offInt(r.read(), 42)
}

func checkAStructInADarrayIsTheSameObject() -> Double {
    let all: MutableList<Counter> = MutableList()
    let a: Counter = Counter(1, 0)
    all.append(a)
    a.n = 6
    return offInt(all[0].n, 6)
}

func checkAStructInAClosureIsShared() -> Double {
    let a: Counter = Counter(1, 0)
    let f: () -> Int64 = { () -> Int64 in
        a.n = a.n &+ 1
        return a.n
    }
    f()
    return offInt(a.n, 2)
}

func makeCounter(_ start: Int64) -> Counter {
    let c: Counter = Counter(start, 1)
    c.n = c.n &+ 1
    return c
}

func lend(_ c: Counter) -> Int64 {
    return c.n &* 2
}

func keepIn(_ store: MutableList<Counter>, _ c: Counter) -> Int64 {
    store.append(c)
    return 0
}

func checkAReturnedObjectSurvives() -> Double {
    let a: Counter = makeCounter(5)
    let b: Counter = makeCounter(10)
    return offInt(a.n &* 100 &+ b.n, 611)
}

func checkAnObjectLentToACalleeIsUsableAfterwards() -> Double {
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 100 {
            let c: Counter = Counter(i, 0)
            total = (total &+ lend(c)) &+ c.n
            c.n = 0
            i = i &+ 1
        }
    }
    return offInt(total, 3 &* 4950)
}

func checkAnObjectAnotherCalleeKeepsSurvives() -> Double {
    let store: MutableList<Counter> = MutableList()
    do {
        var i: Int64 = 0
        while i < 4 {
            let c: Counter = Counter(i &* 10, 0)
            keepIn(store, c)
            i = i &+ 1
        }
    }
    return offInt(((store[0].n &+ store[1].n) &+ store[2].n) &+ store[3].n, 60)
}

func checkAValParameterCopyIsReleasedAndTheOriginalIsNot() -> Double {
    let a: Counter = Counter(1, 0)
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 50 {
            total = total &+ addTenOnACopy(a)
            i = i &+ 1
        }
    }
    return offInt(total, 50 &* 11) + offInt(a.n, 1)
}

func earlyReturnWithObjects(_ n: Int64) -> Int64 {
    let c: Counter = Counter(n, 0)
    if n > 5 {
        return c.n &* 2
    }
    c.bump()
    return c.n
}

func checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue() -> Double {
    return offInt(earlyReturnWithObjects(9) &* 100 &+ earlyReturnWithObjects(2), 1800 &+ 3)
}

func checkLetVarAndValOnReferenceStructs() -> Double {
    let a: Counter = Counter(1, 0)
    let alias: Counter = a
    let copy: Counter = a.copy()
    var moving: Counter = a
    a.n = 10
    moving = Counter(100, 0)
    moving.n = moving.n &+ 1
    return offInt(alias.n, 10) + offInt(copy.n, 1) + offInt(moving.n, 101) + offInt(a.n, 10)
}

func checkValOnAContainerIsAnIndependentCopy() -> Double {
    let d: MutableList<Int64> = MutableList()
    d.append(4)
    let e: MutableList<Int64> = MutableList(d.items)
    d.append(5)
    return offInt(e.count, 1) + offInt(d.count, 2)
}

final class Dot: Hashable {
    let x: Int64
    let y: Int64

    init(_ x: Int64, _ y: Int64) {
        self.x = x
        self.y = y
    }

    static func == (a: Dot, b: Dot) -> Bool {
        [true, a.x == b.x, a.y == b.y].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(x)
        hasher.combine(y)
    }
}

final class Span: Hashable {
    let lo: Int64
    let hi: Int64

    init(_ lo: Int64, _ hi: Int64) {
        self.lo = lo
        self.hi = hi
    }

    func width() -> Int64 {
        return self.hi &- self.lo
    }

    static func == (a: Span, b: Span) -> Bool {
        [true, a.lo == b.lo, a.hi == b.hi].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(lo)
        hasher.combine(hi)
    }
}

final class Meter: Hashable {
    var reading: Int64
    let id: Int64

    init(_ reading: Int64, _ id: Int64) {
        self.reading = reading
        self.id = id
    }

    func bump() -> Int64 {
        self.reading = self.reading &+ 1
        return self.reading
    }

    func copy() -> Meter {
        Meter(reading, id)
    }

    static func == (a: Meter, b: Meter) -> Bool {
        [true, a.reading == b.reading, a.id == b.id].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(reading)
        hasher.combine(id)
    }
}

final class Plate: Reading, Hashable {
    let code: Int64

    init(_ code: Int64) {
        self.code = code
    }

    func read() -> Int64 {
        return self.code
    }

    static func == (a: Plate, b: Plate) -> Bool {
        [true, a.code == b.code].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(code)
    }
}

func checkShorthandStructsWork() -> Double {
    let d: Dot = Dot(3, 4)
    let s: Span = Span(2, 9)
    let w: Wrapper<Int64> = Wrapper(5)
    let r: any Reading = Plate(7)
    return offInt(d.x &+ d.y, 7) + offInt(s.width(), 7) + offInt(w.value, 5) + offInt(r.read(), 7)
}

func checkAShorthandStructWithAVarFieldIsAReference() -> Double {
    let m: Meter = Meter(1, 2)
    let alias: Meter = m
    alias.bump()
    return offInt(m.reading, 2) + offInt(m.id, 2)
}

func checkAHashmapOfObjectsIsUsableThenReleased() -> Double {
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 100 {
            let m: MutableMap<Int64, Counter> = MutableMap()
            m[1] = Counter(i, 1)
            m[2] = Counter(5, 2)
            m[2] = Counter(6, 2)
            m[1]!.bump()
            total = (((total &+ m[1]!.n) &- i) &+ m[2]!.n) &+ m.count
            i = i &+ 1
        }
    }
    return offInt(total, 100 &* ((1 &+ 6) &+ 2))
}

func checkASparseOfStringsIsUsableThenReleased() -> Double {
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 100 {
            let s: Sparse<String> = Sparse("")
            s.set("a" + "b", 0, 0)
            s.set("c" + "d", 2, 3)
            if s.get(2, 3) == "cd" {
                total = total &+ s.count
            }
            i = i &+ 1
        }
    }
    return offInt(total, 200)
}

final class Cell<T: Hashable>: Hashable {
    var v: T

    init(_ v: T) {
        self.v = v
    }

    func copy() -> Cell {
        Cell(v)
    }

    static func == (a: Cell, b: Cell) -> Bool {
        [true, a.v == b.v].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(v)
    }
}

final class Wrapper<T: Hashable>: Hashable {
    let value: T

    init(_ value: T) {
        self.value = value
    }

    static func == (a: Wrapper, b: Wrapper) -> Bool {
        [true, a.value == b.value].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(value)
    }
}
