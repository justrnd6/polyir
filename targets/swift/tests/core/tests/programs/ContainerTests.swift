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

// An `either`: an ok value or an error.
enum Either<T, E> {
    case ok(T)
    case err(E)

    var isOk: Bool {
        if case .ok = self { return true }
        return false
    }

    var okValue: T {
        if case .ok(let value) = self { return value }
        fatalError("not an ok value")
    }

    var errValue: E {
        if case .err(let value) = self { return value }
        fatalError("not an error")
    }
}

extension Either: Equatable where T: Equatable, E: Equatable {}

extension Either: Hashable where T: Hashable, E: Hashable {}

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

// A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(value, i, j)`
// writes one.
final class Grid<T> {
    let dims: [Int]
    var cells: [T]

    init(_ dims: [Int], _ zero: T) {
        self.dims = dims
        cells = Array(repeating: zero, count: dims.reduce(1, *))
    }

    private func place(_ indices: [Int64]) -> Int {
        zip(dims, indices).reduce(0) { at, pair in at * pair.0 + Int(pair.1) }
    }

    func get(_ indices: Int64...) -> T {
        cells[place(indices)]
    }

    func set(_ value: T, _ indices: Int64...) {
        cells[place(indices)] = value
    }

    func copy() -> Grid<T> {
        let out = Grid(dims, cells[0])
        out.cells = cells
        return out
    }
}

// What `throw` throws: the code thrown.
struct Thrown: Error {
    let value: Int64
}

final class Bag: Hashable {
    let items: MutableList<Int64>
    let total: Int64

    init(_ items: MutableList<Int64>, _ total: Int64) {
        self.items = items
        self.total = total
    }

    static func == (a: Bag, b: Bag) -> Bool {
        [true, a.items == b.items, a.total == b.total].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
        hasher.combine(total)
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

func pushThree(_ xs: MutableList<Int64>) -> Int64 {
    xs.append(3)
    return xs.count
}

func pushMany(_ xs: MutableList<Int64>, _ count: Int64) -> Int64 {
    do {
        var i: Int64 = 0
        while i < count {
            xs.append(i &* 2)
            i = i &+ 1
        }
    }
    return xs.count
}

func checkAPushInsideAFunctionIsSeenByTheCaller() -> Double {
    let xs: MutableList<Int64> = MutableList()
    xs.append(1)
    pushThree(xs)
    return offInt(xs.count, 2) + offInt(xs[1], 3)
}

func checkManyPushesInsideAFunctionLeaveTheCallerValid() -> Double {
    let xs: MutableList<Int64> = MutableList()
    xs.append(7)
    pushMany(xs, 100)
    return offInt(xs.count, 101) + offInt(xs[0], 7) + offInt(xs[1], 0) + offInt(xs[100], 198)
}

func checkTwoNamesForOneDarrayShareIt() -> Double {
    let a: MutableList<Int64> = MutableList()
    let b: MutableList<Int64> = a
    a.append(5)
    b.append(6)
    return offInt(a.count, 2) + offInt(b.count, 2) + offInt(a[1], 6) + offInt(b[0], 5)
}

func checkAnElementWrittenInAFunctionIsSeenByTheCaller() -> Double {
    let xs: MutableList<Int64> = MutableList()
    xs.append(1)
    xs.append(2)
    overwriteFirst(xs, 40)
    return offInt(xs[0], 40) + offInt(xs[1], 2)
}

func overwriteFirst(_ xs: MutableList<Int64>, _ value: Int64) -> Int64 {
    xs[0] = value
    return 0
}

func makeFilled(_ count: Int64) -> MutableList<Int64> {
    let xs: MutableList<Int64> = MutableList()
    do {
        var i: Int64 = 0
        while i < count {
            xs.append(i &+ 1)
            i = i &+ 1
        }
    }
    return xs
}

func checkAReturnedDarrayIsTheOneTheFunctionBuilt() -> Double {
    let xs: MutableList<Int64> = makeFilled(4)
    xs.append(50)
    return offInt(xs.count, 5) + offInt(xs[3], 4) + offInt(xs[4], 50)
}

func checkAStructFieldIsTheSameDarray() -> Double {
    let xs: MutableList<Int64> = MutableList()
    let bag: Bag = Bag(xs, 0)
    xs.append(8)
    bag.items.append(9)
    return offInt(bag.items.count, 2) + offInt(xs.count, 2) + offInt(bag.items[1], 9)
}

func checkAMethodStylePushOnAFieldWorks() -> Double {
    let bag: Bag = Bag(MutableList(), 0)
    bag.items.append(4)
    bag.items.append(5)
    return offInt(bag.items.count, 2) + offInt(bag.items[1], 5)
}

func checkConcatenationMakesANewDarray() -> Double {
    let a: MutableList<Int64> = MutableList()
    let b: MutableList<Int64> = MutableList()
    a.append(1)
    b.append(2)
    let c: MutableList<Int64> = concat(a, b)
    a.append(99)
    return offInt(c.count, 2) + offInt(c[0], 1) + offInt(c[1], 2) + offInt(a.count, 2)
}

func checkAClosureSharesTheDarrayItCaptures() -> Double {
    let xs: MutableList<Int64> = MutableList()
    let add: (Int64) -> Int64 = { (v: Int64) -> Int64 in
        xs.append(v)
        return xs.count
    }
    add(10)
    add(20)
    return offInt(xs.count, 2) + offInt(xs[1], 20)
}

func checkPushGivesBackTheSameDarray() -> Double {
    var xs: MutableList<Int64> = MutableList()
    xs = pushed(xs, 5)
    xs = pushed(xs, 6)
    return offInt(xs.count, 2) + offInt(xs[1], 6)
}

func checkReassigningAVarSwitchesDarrays() -> Double {
    var xs: MutableList<Int64> = MutableList()
    let other: MutableList<Int64> = MutableList()
    other.append(1)
    other.append(2)
    other.append(3)
    xs = other
    xs.append(4)
    return offInt(other.count, 4) + offInt(xs.count, 4)
}

func checkAListOfListsSharesItsInnerLists() -> Double {
    let outer: MutableList<MutableList<Int64>> = MutableList()
    let a: MutableList<Int64> = MutableList()
    a.append(1)
    a.append(2)
    outer.append(a)
    let b: MutableList<Int64> = MutableList()
    b.append(10)
    outer.append(b)
    a.append(3)
    let first: MutableList<Int64> = outer[0]
    let second: MutableList<Int64> = outer[1]
    return offInt(outer.count, 2) + offInt(first.count, 3) + offInt(first[2], 3) + offInt(second[0], 10)
}

func checkADarrayOfStructsKeepsTheStructs() -> Double {
    let ps: MutableList<Pt> = MutableList()
    ps.append(Pt(1, 2))
    ps.append(Pt(3, 4))
    let p: Pt = ps[1]
    return offInt(ps.count, 2) + offInt(p.x, 3) + offInt(p.y, 4)
}

func checkAThousandPushesKeepEveryElement() -> Double {
    let xs: MutableList<Int64> = MutableList()
    do {
        var i: Int64 = 0
        while i < 1000 {
            xs.append(i)
            i = i &+ 1
        }
    }
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < xs.count {
            total = total &+ xs[i]
            i = i &+ 1
        }
    }
    return offInt(xs.count, 1000) + offInt(total, 499500)
}

func checkAListInsideAnEitherIsTheSameList() -> Double {
    let xs: MutableList<Int64> = MutableList()
    xs.append(4)
    let r: Either<MutableList<Int64>, Int64> = Either<MutableList<Int64>, Int64>.ok(xs)
    xs.append(5)
    let back: MutableList<Int64> = r.okValue
    return offInt(back.count, 2) + offInt(back[1], 5)
}

func fillSparse(_ sp: Sparse<Int64>) -> Int64 {
    sp.set(5, 1, 1)
    sp.set(7, 2, 3)
    return 0
}

func checkASparseWrittenInAFunctionIsSeenByTheCaller() -> Double {
    let sp: Sparse<Int64> = Sparse(0)
    fillSparse(sp)
    return offInt(sp.get(1, 1), 5) + offInt(sp.get(2, 3), 7) + offInt(sp.get(9, 9), 0) + offInt(sp.count, 2)
}

func checkLastWriteWinsForASparse() -> Double {
    let sp: Sparse<Int64> = Sparse(0)
    sp.set(1, 3, 3)
    sp.set(2, 3, 3)
    sp.set(3, 3, 3)
    return offInt(sp.get(3, 3), 3)
}

func checkOneDimensionalSparseSharesToo() -> Double {
    let sp: Sparse<Int64> = Sparse(0)
    let alias: Sparse<Int64> = sp
    alias.set(100, 0, 0)
    sp.set(200, 7, 0)
    return offInt(sp.get(0, 0), 100) + offInt(alias.get(7, 0), 200)
}

func fillMap(_ hm: MutableMap<Int64, Int64>) -> Int64 {
    hm[1] = 10
    hm[2] = 20
    hm[1] = 11
    return 0
}

func checkAHashmapWrittenInAFunctionIsSeenByTheCaller() -> Double {
    let hm: MutableMap<Int64, Int64> = MutableMap()
    fillMap(hm)
    return offInt(hm[1] ?? 0, 11) + offInt(hm[2] ?? 0, 20) + offInt(hm[3] ?? 0, 0) + offInt(hm.count, 2)
}

func checkTwoNamesForOneHashmapShareIt() -> Double {
    let a: MutableMap<Int64, Int64> = MutableMap()
    let b: MutableMap<Int64, Int64> = a
    a[5] = 50
    b[6] = 60
    return offInt(a.count, 2) + offInt(b[5] ?? 0, 50) + offInt(a[6] ?? 0, 60)
}

func checkAStringKeyedHashmapIsShared() -> Double {
    let hm: MutableMap<String, Int64> = MutableMap()
    addWord(hm, "one", 1)
    addWord(hm, "two", 2)
    return offInt(hm["one"] ?? 0, 1) + offInt(hm["two"] ?? 0, 2) + offInt(hm.count, 2)
}

func addWord(_ hm: MutableMap<String, Int64>, _ word: String, _ value: Int64) -> Int64 {
    hm[word] = value
    return 0
}

func checkAHashmapOfListsKeepsTheLists() -> Double {
    let hm: MutableMap<Int64, MutableList<Int64>> = MutableMap()
    let l: MutableList<Int64> = MutableList()
    hm[7] = l
    l.append(5)
    l.append(6)
    let got: MutableList<Int64> = hm[7] ?? MutableList()
    return offInt(got.count, 2) + offInt(got[1], 6)
}

func fillGrid(_ g: Grid<Int64>) -> Int64 {
    do {
        var r: Int64 = 0
        while r < 2 {
            do {
                var c: Int64 = 0
                while c < 3 {
                    g.set(r &* 10 &+ c, r, c)
                    c = c &+ 1
                }
            }
            r = r &+ 1
        }
    }
    return 0
}

func checkAGridFilledInAFunctionIsSeenByTheCaller() -> Double {
    let g: Grid<Int64> = Grid([2, 3], 0)
    fillGrid(g)
    return offInt(g.get(1, 2), 12) + offInt(g.get(0, 1), 1)
}

func checkTwoNamesForOneGridShareIt() -> Double {
    let a: Grid<Int64> = Grid([2, 3], 0)
    let b: Grid<Int64> = a
    a.set(9, 1, 1)
    return offInt(b.get(1, 1), 9)
}

func checkAGridStartsZeroed() -> Double {
    let t: Grid<Int64> = Grid([2, 2, 2], 0)
    t.set(5, 1, 1, 1)
    return offInt(t.get(0, 1, 0), 0) + offInt(t.get(1, 1, 1), 5)
}

func checkDarrayCopyIsIndependent() -> Double {
    let a: MutableList<Int64> = MutableList()
    a.append(1)
    a.append(2)
    let b: MutableList<Int64> = MutableList(a.items)
    b.append(3)
    a.append(9)
    return offInt(a.count, 3) + offInt(b.count, 3) + offInt(b[2], 3) + offInt(a[2], 9) + offInt(b[0], 1)
}

func checkGridAndTensorCopyAreIndependent() -> Double {
    let g: Grid<Int64> = Grid([2, 2], 0)
    g.set(5, 1, 1)
    let h: Grid<Int64> = g.copy()
    g.set(6, 1, 1)
    let t: Grid<Int64> = Grid([2, 2, 2], 0)
    t.set(7, 1, 0, 1)
    let u: Grid<Int64> = t.copy()
    t.set(8, 1, 0, 1)
    return offInt(h.get(1, 1), 5) + offInt(g.get(1, 1), 6) + offInt(u.get(1, 0, 1), 7)
}

func checkSparseCopyIsIndependent() -> Double {
    let s: Sparse<Int64> = Sparse(0)
    s.set(7, 1, 2)
    let c: Sparse<Int64> = s.copy()
    s.set(8, 3, 4)
    c.set(9, 5, 6)
    return offInt(s.count, 2) + offInt(c.count, 2) + offInt(c.get(1, 2), 7)
}

func checkHashmapCopyIsIndependent() -> Double {
    let m: MutableMap<Int64, Int64> = MutableMap()
    m[1] = 10
    let c: MutableMap<Int64, Int64> = MutableMap(m.items)
    m[2] = 20
    c[3] = 30
    return offInt(m.count, 2) + offInt(c.count, 2) + offInt(c[1] ?? 0, 10) + offInt(c[3] ?? 0, 30)
}

func checkCopyOfAStringKeyedHashmap() -> Double {
    let m: MutableMap<String, Int64> = MutableMap()
    m["a"] = 1
    let c: MutableMap<String, Int64> = MutableMap(m.items)
    m["b"] = 2
    return offInt(c["a"] ?? 0, 1) + offInt(c.count, 1)
}

func makeBag(_ n: Int64) -> MutableList<Int64> {
    let d: MutableList<Int64> = MutableList()
    d.append(n)
    d.append(n &+ 1)
    return d
}

func sumGrid(_ g: Grid<Int64>) -> Int64 {
    return g.get(0, 0) &+ g.get(1, 1)
}

func holdsOnto(_ store: MutableList<MutableList<Int64>>, _ d: MutableList<Int64>) -> Int64 {
    store.append(d)
    return 0
}

func checkAReturnedContainerSurvives() -> Double {
    let a: MutableList<Int64> = makeBag(10)
    let b: MutableList<Int64> = makeBag(20)
    return offInt(((a[0] &+ a[1]) &+ b[0]) &+ b[1], 62)
}

func checkAContainerLentToACalleeIsStillUsableAfterwards() -> Double {
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 50 {
            let g: Grid<Int64> = Grid([2, 2], 0)
            g.set(i, 0, 0)
            g.set(1, 1, 1)
            total = (total &+ sumGrid(g)) &+ g.get(0, 0)
            i = i &+ 1
        }
    }
    return offInt(total, (2 &* 1225 &+ 50) &+ 0)
}

func checkAContainerGivenToACalleeThatKeepsItSurvives() -> Double {
    let store: MutableList<MutableList<Int64>> = MutableList()
    do {
        var i: Int64 = 0
        while i < 3 {
            let d: MutableList<Int64> = MutableList()
            d.append(i &* 10)
            holdsOnto(store, d)
            i = i &+ 1
        }
    }
    return offInt((store[0][0] &+ store[1][0]) &+ store[2][0], 30)
}

func checkAContainerReturnedFromInsideAnIfSurvives() -> Double {
    let keep: MutableList<Int64> = MutableList()
    keep.append(4)
    let other: MutableList<Int64> = MutableList()
    other.append(5)
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 20 {
            let scratch: MutableList<Int64> = MutableList()
            scratch.append(i)
            if i == 7 {
                total = total &+ scratch[0]
            }
            i = i &+ 1
        }
    }
    return offInt((total &+ keep[0]) &+ other[0], 16)
}

func earlyExit(_ n: Int64) -> Int64 {
    let scratch: MutableList<Int64> = MutableList()
    scratch.append(n)
    if n > 5 {
        return scratch[0] &* 2
    }
    scratch.append(1)
    return scratch.count &+ scratch[0]
}

func checkAReturnInsideAnIfStillReleasesAndReturnsTheRightValue() -> Double {
    return offInt(earlyExit(9) &* 100 &+ earlyExit(2), 18 &* 100 &+ 4)
}

func greeting(_ name: String) -> String {
    let hello: String = "hello, "
    let full: String = hello + name + "!"
    return full
}

func shout(_ s: String) -> Int64 {
    if s == "hi!" {
        return 1
    }
    return 0
}

func counterFrom(_ start: Int64) -> () -> Int64 {
    var n: Int64 = start
    let next: () -> Int64 = { () -> Int64 in
        n = n &+ 1
        return n
    }
    return next
}

func applyTwice(_ f: @escaping () -> Int64) -> Int64 {
    f()
    return f()
}

func checkAReturnedStringSurvives() -> Double {
    let a: String = greeting("ann")
    let b: String = greeting("bo")
    if a != "hello, ann!" {
        return 1.0
    }
    if b != "hello, bo!" {
        return 1.0
    }
    return 0.0
}

func checkAStringLentToACalleeIsUsableAfterwards() -> Double {
    var hits: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 20 {
            let s: String = "h" + "i" + "!"
            hits = hits &+ shout(s)
            if s == "hi!" {
                hits = hits &+ 1
            }
            i = i &+ 1
        }
    }
    return offInt(hits, 40)
}

func checkAStringUsedAsAHashmapKeyIsKept() -> Double {
    let m: MutableMap<String, Int64> = MutableMap()
    do {
        var i: Int64 = 0
        while i < 3 {
            let key: String = "k" + "ey"
            m[key] = i
            i = i &+ 1
        }
    }
    return offInt(m["key"] ?? 0, 2) + offInt(m.count, 1)
}

func checkAClosureThatIsReturnedKeepsItsBox() -> Double {
    let c: () -> Int64 = counterFrom(10)
    c()
    return offInt(c(), 12)
}

func checkAClosureGivenToACalleeKeepsWorking() -> Double {
    var n: Int64 = 0
    let tick: () -> Int64 = { () -> Int64 in
        n = n &+ 5
        return n
    }
    return offInt(applyTwice(tick), 10) + offInt(n, 10)
}

func checkAClosureOnlyCalledLocallyInALoop() -> Double {
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 50 {
            var count: Int64 = i
            let bump: () -> Int64 = { () -> Int64 in
                count = count &+ 1
                return count
            }
            bump()
            total = (total &+ bump()) &- i
            i = i &+ 1
        }
    }
    return offInt(total, 100)
}

func checkTwoClosuresShareOneCapturedName() -> Double {
    var n: Int64 = 0
    let inc: () -> Int64 = { () -> Int64 in
        n = n &+ 1
        return n
    }
    let read: () -> Int64 = { () in n &* 10 }
    inc()
    inc()
    return offInt(read(), 20)
}

final class Pocket: Hashable {
    var items: MutableList<Int64>

    init(_ items: MutableList<Int64>) {
        self.items = items
    }

    func copy() -> Pocket {
        Pocket(items)
    }

    static func == (a: Pocket, b: Pocket) -> Bool {
        [true, a.items == b.items].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

func madeHere() -> MutableList<Int64> {
    let d: MutableList<Int64> = MutableList()
    d.append(1)
    return d
}

func madeThroughAnother() -> MutableList<Int64> {
    let d: MutableList<Int64> = madeHere()
    d.append(2)
    return d
}

func madeInPlace() -> MutableList<Int64> {
    return MutableList()
}

func sameAsArgument(_ d: MutableList<Int64>) -> MutableList<Int64> {
    return d
}

func elementOf(_ xs: MutableList<MutableList<Int64>>) -> MutableList<Int64> {
    return xs[0]
}

func fieldOf(_ p: Pocket) -> MutableList<Int64> {
    return p.items
}

func keptAndReturned(_ store: MutableList<MutableList<Int64>>) -> MutableList<Int64> {
    let d: MutableList<Int64> = MutableList()
    store.append(d)
    return d
}

func sometimesTheArgument(_ flag: Int64, _ d: MutableList<Int64>) -> MutableList<Int64> {
    if flag == 0 {
        return d
    }
    return MutableList()
}

func checkAFreshResultIsUsableAndReleased() -> Double {
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 100 {
            let a: MutableList<Int64> = madeHere()
            let b: MutableList<Int64> = madeThroughAnother()
            let c: MutableList<Int64> = madeInPlace()
            c.append(i)
            total = ((total &+ a.count) &+ b.count) &+ c.count
            i = i &+ 1
        }
    }
    return offInt(total, 100 &* 4)
}

func checkAnAliasReturnedByACallIsNotReleasedUnderItsOwner() -> Double {
    let src: MutableList<Int64> = MutableList()
    src.append(7)
    let same: MutableList<Int64> = sameAsArgument(src)
    same.append(8)
    let outer: MutableList<MutableList<Int64>> = MutableList()
    outer.append(src)
    let inner: MutableList<Int64> = elementOf(outer)
    let pocket: Pocket = Pocket(src)
    let field: MutableList<Int64> = fieldOf(pocket)
    let maybe: MutableList<Int64> = sometimesTheArgument(0, src)
    return offInt(src.count, 2) + offInt(inner.count, 2) + offInt(field.count, 2) + offInt(maybe.count, 2)
}

func checkAnObjectKeptByTheCalleeSurvivesItsCaller() -> Double {
    let store: MutableList<MutableList<Int64>> = MutableList()
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 5 {
            let d: MutableList<Int64> = keptAndReturned(store)
            d.append(i)
            i = i &+ 1
        }
    }
    do {
        var j: Int64 = 0
        while j < 5 {
            total = total &+ store[j][0]
            j = j &+ 1
        }
    }
    return offInt(total, 10)
}

func handedOn() -> MutableList<Int64> {
    let a: MutableList<Int64> = madeHere()
    return a
}

func checkAResultHandedOnAgainIsStillFresh() -> Double {
    let a: MutableList<Int64> = handedOn()
    return offInt(a[0], 1)
}

func checkADiscardedFreshResultIsHarmless() -> Double {
    madeHere()
    madeThroughAnother()
    return 0.0
}

func madeStruct(_ n: Int64) -> Pocket {
    let p: Pocket = Pocket(MutableList())
    p.items.append(n)
    return p
}

func makeLabel(_ a: String) -> String {
    let s: String = a + "-" + a
    return s
}

func checkAStructAndAStringHandedOverByACall() -> Double {
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 50 {
            let p: Pocket = madeStruct(i)
            let s: String = makeLabel("x")
            if s == "x-x" {
                total = (total &+ p.items[0]) &+ 1
            }
            i = i &+ 1
        }
    }
    return offInt(total, 1225 &+ 50)
}

final class Shelf: Hashable {
    var books: MutableList<Int64>
    var tags: String

    init(_ books: MutableList<Int64>, _ tags: String) {
        self.books = books
        self.tags = tags
    }

    func copy() -> Shelf {
        Shelf(books, tags)
    }

    static func == (a: Shelf, b: Shelf) -> Bool {
        [true, a.books == b.books, a.tags == b.tags].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(books)
        hasher.combine(tags)
    }
}

final class Cabinet: Hashable {
    var drawer: MutableList<Int64>

    init(_ drawer: MutableList<Int64>) {
        self.drawer = drawer
    }

    func copy() -> Cabinet {
        Cabinet(drawer)
    }

    static func == (a: Cabinet, b: Cabinet) -> Bool {
        [true, a.drawer == b.drawer].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(drawer)
    }
}

final class Crate: Hashable {
    var items: MutableList<Int64>

    init(_ items: MutableList<Int64>) {
        self.items = items
    }

    func copy() -> Crate {
        Crate(items)
    }

    static func == (a: Crate, b: Crate) -> Bool {
        [true, a.items == b.items].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

func stockedShelf(_ n: Int64) -> Shelf {
    let s: Shelf = Shelf(MutableList(), "a" + "b")
    s.books.append(n)
    s.books.append(n &+ 1)
    return s
}

func openDrawer(_ c: Cabinet) -> MutableList<Int64> {
    return c.drawer
}

func checkAnOwnedFieldIsUsableThenReleasedWithItsStruct() -> Double {
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 100 {
            let s: Shelf = stockedShelf(i)
            total = ((total &+ s.books.count) &+ s.books[0]) &- i
            if s.tags == "ab" {
                total = total &+ 1
            }
            i = i &+ 1
        }
    }
    return offInt(total, 100 &* 3)
}

func checkAReplacedFieldValueIsReleasedAndTheNewOneWorks() -> Double {
    let s: Shelf = stockedShelf(5)
    s.books = MutableList()
    s.books.append(40)
    s.tags = "x" + "y"
    if s.tags != "xy" {
        return 1.0
    }
    return offInt(s.books.count, 1) + offInt(s.books[0], 40)
}

func checkAFieldThatIsReadOutIsNeverReleasedWithItsStruct() -> Double {
    let c: Cabinet = Cabinet(MutableList())
    c.drawer.append(9)
    let drawer: MutableList<Int64> = openDrawer(c)
    drawer.append(10)
    return offInt(c.drawer.count, 2) + offInt(drawer[1], 10)
}

func checkACopiedStructSharesNothingItMustNotFree() -> Double {
    let a: Crate = Crate(MutableList())
    a.items.append(3)
    let b: Crate = a.copy()
    b.items.append(4)
    return offInt(a.items.count, 2) + offInt(b.items.count, 2)
}

func checkAListOfListsReleasesItsElements() -> Double {
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 100 {
            let outer: MutableList<MutableList<Int64>> = MutableList()
            do {
                var j: Int64 = 0
                while j < 3 {
                    outer.append(MutableList())
                    outer[j].append(i &+ j)
                    j = j &+ 1
                }
            }
            total = (((total &+ outer.count) &+ outer[2].count) &+ outer[1][0]) &- i
            i = i &+ 1
        }
    }
    return offInt(total, 100 &* ((3 &+ 1) &+ 1))
}

func firstOf(_ xs: MutableList<MutableList<Int64>>) -> MutableList<Int64> {
    let first: MutableList<Int64> = xs[0]
    return first
}

func checkAnElementReadOutOfAListIsNotReleasedWithIt() -> Double {
    let store: MutableList<MutableList<Int64>> = MutableList()
    store.append(MutableList())
    store[0].append(6)
    let got: MutableList<Int64> = firstOf(store)
    got.append(7)
    return offInt(store[0].count, 2) + offInt(got[1], 7)
}

func checkAMapPastItsStartingCapacityKeepsEveryEntry() -> Double {
    let m: MutableMap<Int64, Int64> = MutableMap()
    do {
        var i: Int64 = 0
        while i < 1000 {
            m[i] = i &* 3
            i = i &+ 1
        }
    }
    var wrong: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 1000 {
            if m[i] ?? 0 != i &* 3 {
                wrong = wrong &+ 1
            }
            i = i &+ 1
        }
    }
    return offInt(wrong, 0) + offInt(m.count, 1000)
}

func checkAGrownMapIsStillTheSameMapForItsAliases() -> Double {
    let m: MutableMap<Int64, Int64> = MutableMap()
    let alias: MutableMap<Int64, Int64> = m
    do {
        var i: Int64 = 0
        while i < 20 {
            alias[i] = i
            i = i &+ 1
        }
    }
    return offInt(m.count, 20) + offInt(m[19] ?? 0, 19)
}

func checkStringKeysSurviveGrowing() -> Double {
    let m: MutableMap<String, Int64> = MutableMap()
    m["a"] = 1
    m["b"] = 2
    m["c"] = 3
    m["d"] = 4
    m["a"] = 10
    return offInt(m["a"] ?? 0, 10) + offInt(m["b"] ?? 0, 2) + offInt(m["c"] ?? 0, 3) + offInt(m["d"] ?? 0, 4) + offInt(m.count, 4)
}

final class GrowKey: Hashable {
    let a: Int64
    let b: Int64

    init(_ a: Int64, _ b: Int64) {
        self.a = a
        self.b = b
    }

    static func == (a: GrowKey, b: GrowKey) -> Bool {
        [true, a.a == b.a, a.b == b.b].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(a)
        hasher.combine(b)
    }
}

func checkStructKeysSurviveGrowing() -> Double {
    let m: MutableMap<GrowKey, Int64> = MutableMap()
    do {
        var i: Int64 = 0
        while i < 50 {
            m[GrowKey(i, i &+ 1)] = i
            i = i &+ 1
        }
    }
    var wrong: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 50 {
            if m[GrowKey(i, i &+ 1)] ?? 0 != i {
                wrong = wrong &+ 1
            }
            i = i &+ 1
        }
    }
    return offInt(wrong, 0) + offInt(m.count, 50)
}

func checkASetPastItsStartingCapacityKeepsEveryMember() -> Double {
    let seen: MutableSet<Int64> = MutableSet()
    do {
        var i: Int64 = 0
        while i < 100 {
            seen.items.insert(i % 40)
            i = i &+ 1
        }
    }
    var absent: Int64 = 0
    if seen.items.contains(39) == false {
        absent = absent &+ 1
    }
    if seen.items.contains(40) {
        absent = absent &+ 1
    }
    return offInt(seen.count, 40) + offInt(absent, 0)
}

func readAt(_ xs: MutableList<Int64>, _ i: Int64) -> Int64 {
    return xs[i]
}

func caughtCode(_ xs: MutableList<Int64>, _ i: Int64) -> Int64 {
    var code: Int64 = 0
    do {
        code = readAt(xs, i)
    } catch {
        let c = (error as! Thrown).value
        code = c
    }
    return code
}

func checkAnIndexPastTheEndOfAListThrowsMinusOne() -> Double {
    let xs: MutableList<Int64> = MutableList()
    xs.append(5)
    return offInt(caughtCode(xs, 1), 0 &- 1) + offInt(caughtCode(xs, 100000), 0 &- 1)
}

func checkANegativeIndexThrowsMinusOne() -> Double {
    let xs: MutableList<Int64> = MutableList()
    xs.append(5)
    return offInt(caughtCode(xs, 0 &- 1), 0 &- 1)
}

func checkAnIndexInsideTheListStillReads() -> Double {
    let xs: MutableList<Int64> = MutableList()
    xs.append(5)
    xs.append(6)
    return offInt(caughtCode(xs, 0), 5) + offInt(caughtCode(xs, 1), 6)
}

func checkAnEmptyListCannotBeRead() -> Double {
    let xs: MutableList<Int64> = MutableList()
    return offInt(caughtCode(xs, 0), 0 &- 1)
}

func divideBy(_ a: Int64, _ b: Int64) -> Int64 {
    var out: Int64 = 0
    do {
        out = a / b
    } catch {
        let c = (error as! Thrown).value
        out = c
    }
    return out
}

func checkDividingByZeroThrowsMinusFour() -> Double {
    return offInt(divideBy(10, 0), 0 &- 4) + offInt(divideBy(10, 2), 5)
}

func checkAFixedArrayAndAGridAreCheckedPerDimension() -> Double {
    var code: Int64 = 0
    let g: Grid<Int64> = Grid([2, 2], 0)
    do {
        g.set(1, 0, 5)
    } catch {
        let c = (error as! Thrown).value
        code = c
    }
    return offInt(code, 0 &- 1)
}

final class OptPoint: Hashable {
    let x: Int64
    let y: Int64

    init(_ x: Int64, _ y: Int64) {
        self.x = x
        self.y = y
    }

    static func == (a: OptPoint, b: OptPoint) -> Bool {
        [true, a.x == b.x, a.y == b.y].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(x)
        hasher.combine(y)
    }
}

func optionalFind(_ xs: MutableList<Int64>, _ want: Int64) -> Int64? {
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

func optionalPoint(_ ok: Int64) -> (OptPoint)? {
    if ok == 1 {
        return OptPoint(7, 8)
    }
    return nil
}

func optionalName(_ ok: Int64) -> String? {
    if ok == 1 {
        return "ann"
    }
    return nil
}

func unwrapCode(_ o: Int64?) -> Int64 {
    var code: Int64 = 0
    do {
        code = o!
    } catch {
        let c = (error as! Thrown).value
        code = c
    }
    return code
}

func checkAnOptionalHoldsAValueOrNone() -> Double {
    let xs: MutableList<Int64> = MutableList()
    xs.append(10)
    xs.append(20)
    var seen: Int64 = 0
    if optionalFind(xs, 20) != nil {
        seen = seen &+ 1
    }
    if optionalFind(xs, 99) == nil {
        seen = seen &+ 10
    }
    return offInt(seen, 11) + offInt(optionalFind(xs, 20)!, 1)
}

func checkAnOptionalOfAStructAndOfAStringWork() -> Double {
    let p: (OptPoint)? = optionalPoint(1)
    var seen: Int64 = 0
    if p != nil {
        seen = (seen &+ p!.x) &+ p!.y
    }
    if optionalPoint(0) == nil {
        seen = seen &+ 100
    }
    if optionalName(1) != nil {
        seen = seen &+ 1000
    }
    return offInt(seen, 1115)
}

func checkUnwrappingNoneThrowsMinusThree() -> Double {
    return offInt(unwrapCode(nil), 0 &- 3) + offInt(unwrapCode(42), 42)
}
