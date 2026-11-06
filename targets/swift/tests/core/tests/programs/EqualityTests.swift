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

// A variant: the number of its case and the value it holds. Two compare by their cases and what their values say.
struct Variant: Equatable {
    let tag: Int64
    let value: Any?

    static func == (a: Variant, b: Variant) -> Bool {
        a.tag == b.tag && String(describing: a.value) == String(describing: b.value)
    }
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

final class Line: Hashable {
    let a: Point
    let b: Point

    init(_ a: Point, _ b: Point) {
        self.a = a
        self.b = b
    }

    static func == (a: Line, b: Line) -> Bool {
        [true, a.a == b.a, a.b == b.b].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(a)
        hasher.combine(b)
    }
}

final class Named: Hashable {
    let name: String
    let tags: MutableList<Int64>

    init(_ name: String, _ tags: MutableList<Int64>) {
        self.name = name
        self.tags = tags
    }

    static func == (a: Named, b: Named) -> Bool {
        [true, a.name == b.name, a.tags == b.tags].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(name)
        hasher.combine(tags)
    }
}

final class Counter: Hashable {
    var n: Int64
    let id: Int64

    init(_ n: Int64, _ id: Int64) {
        self.n = n
        self.id = id
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

final class Digit: Hashable {
    let v: Int64

    init(_ v: Int64) {
        self.v = v
    }

    func equals(_ other: Digit) -> Bool {
        return self.v % 10 == other.v % 10
    }

    static func == (a: Digit, b: Digit) -> Bool {
        [true, a.v == b.v].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(v)
    }
}

final class Wrapped: Hashable {
    let inner: Either<Int64, String>

    init(_ inner: Either<Int64, String>) {
        self.inner = inner
    }

    static func == (a: Wrapped, b: Wrapped) -> Bool {
        [true, a.inner == b.inner].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(inner)
    }
}

func checkStructsAreEqualFieldByField() -> Double {
    let p: Point = Point(1, 2)
    let q: Point = Point(1, 2)
    let r: Point = Point(1, 3)
    var score: Int64 = 0
    if p == q {
        score = score &+ 1
    }
    if p != r {
        score = score &+ 2
    }
    if p == r {
        score = score &+ 100
    }
    return offInt(score, 3)
}

func checkNestedStructsAreEqualRecursively() -> Double {
    let p: Point = Point(1, 2)
    let z: Point = Point(3, 4)
    let a: Line = Line(p, z)
    let b: Line = Line(Point(1, 2), Point(3, 4))
    let c: Line = Line(z, p)
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a != c {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkAStringAndAListInsideAStructAreComparedByContent() -> Double {
    let t1: MutableList<Int64> = MutableList()
    t1.append(1)
    let t2: MutableList<Int64> = MutableList()
    t2.append(1)
    let a: Named = Named("a" + "b", t1)
    let b: Named = Named("ab", t2)
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    t2.append(2)
    if a != b {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkStringsCompareByCharacters() -> Double {
    let x: String = "match"
    let y: String = "ma" + "tch"
    var score: Int64 = 0
    if x == y {
        score = score &+ 1
    }
    if x != "other" {
        score = score &+ 2
    }
    if x == y {
        score = score &+ 100
    }
    return offInt(score, 3)
}

func checkAStructWithVarFieldsIsComparedByWhatItHoldsNow() -> Double {
    let a: Counter = Counter(1, 2)
    let b: Counter = Counter(1, 2)
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    b.n = 5
    if a != b {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkIdentityIsTheSameObject() -> Double {
    let a: Counter = Counter(1, 2)
    let b: Counter = Counter(1, 2)
    let alias: Counter = a
    var score: Int64 = 0
    if a === alias {
        score = score &+ 1
    }
    if a !== b {
        score = score &+ 2
    }
    if a === b {
        score = score &+ 100
    }
    return offInt(score, 3)
}

func checkAnImmutableStructComparedByIdentityIsAnObject() -> Double {
    let p: Point = Point(1, 2)
    let q: Point = Point(1, 2)
    let same: Point = p
    var score: Int64 = 0
    if p === same {
        score = score &+ 1
    }
    if p !== q {
        score = score &+ 2
    }
    if p == q {
        score = score &+ 4
    }
    return offInt(score, 7)
}

func checkAValStructIsADifferentObjectThatIsEqual() -> Double {
    let a: Counter = Counter(1, 2)
    let b: Counter = a.copy()
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a !== b {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkAnEqualsMethodDecidesWhatEqualMeans() -> Double {
    let a: Digit = Digit(13)
    let b: Digit = Digit(23)
    let c: Digit = Digit(24)
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a != c {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkListsAreEqualElementByElement() -> Double {
    let a: MutableList<Int64> = MutableList()
    let b: MutableList<Int64> = MutableList()
    a.append(1)
    a.append(2)
    b.append(1)
    b.append(2)
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    b.append(3)
    if a != b {
        score = score &+ 2
    }
    if a === b {
        score = score &+ 100
    }
    return offInt(score, 3)
}

func checkListsOfStructsAreEqualDeeply() -> Double {
    let a: MutableList<Point> = MutableList()
    let b: MutableList<Point> = MutableList()
    a.append(Point(1, 2))
    b.append(Point(1, 2))
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    a.append(Point(5, 6))
    b.append(Point(5, 7))
    if a != b {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload() -> Double {
    let a: Either<Int64, Int64> = Either<Int64, Int64>.ok(5)
    let b: Either<Int64, Int64> = Either<Int64, Int64>.ok(5)
    let c: Either<Int64, Int64> = Either<Int64, Int64>.err(5)
    let d: Either<Int64, Int64> = Either<Int64, Int64>.ok(6)
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a != c {
        score = score &+ 2
    }
    if a != d {
        score = score &+ 4
    }
    return offInt(score, 7)
}

func checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload() -> Double {
    let a: Variant = Variant(tag: 1, value: 2.5)
    let b: Variant = Variant(tag: 1, value: 2.5)
    let c: Variant = Variant(tag: 0, value: Int64(2))
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a != c {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkAStructHoldingAnEitherIsComparedThroughIt() -> Double {
    let a: Wrapped = Wrapped(Either<Int64, String>.ok(1))
    let b: Wrapped = Wrapped(Either<Int64, String>.ok(1))
    let c: Wrapped = Wrapped(Either<Int64, String>.err("no"))
    var score: Int64 = 0
    if a == b {
        score = score &+ 1
    }
    if a != c {
        score = score &+ 2
    }
    return offInt(score, 3)
}

func checkNanIsNotEqualToItself() -> Double {
    let nan: Double = 0.0 / 0.0
    var score: Int64 = 0
    if nan == nan {
        score = score &+ 100
    }
    if nan != nan {
        score = score &+ 1
    }
    return offInt(score, 1)
}
