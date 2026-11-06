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

func checkConversionAtACall() -> Double {
    return offInt(directCall(), 920)
}

func checkConversionAtAReturn() -> Double {
    return offInt(chosen(), 36034)
}

func checkDarrayOfInterfaceValues() -> Double {
    return offInt(sumAll(), 41)
}

func checkInterfaceAsAStructField() -> Double {
    return offInt(viaField(), 307)
}

func checkOneStructTwoInterfaces() -> Double {
    return offInt(twoInterfaces(), 1016)
}

func checkInterfaceMethodTakingAnInterface() -> Double {
    return offInt(interfaceArgument(), 109)
}

func checkDispatchFollowsTheValue() -> Double {
    let a: any Shape = Square(5)
    let b: any Shape = Rect(5, 2)
    return offInt(a.area(), 25) + offInt(b.area(), 10)
}

func checkSameMethodNameOnTwoStructs() -> Double {
    let s: Square = Square(3)
    let r: Rect = Rect(3, 4)
    return offInt(s.area() &+ r.area(), 21)
}

func checkConvertedValueIsACopy() -> Double {
    let s: Square = Square(4)
    let a: any Shape = s
    let b: any Shape = s
    return offInt(a.area() &+ b.area(), 32)
}

protocol Scaler: AnyObject {
    func half(_ p0: Float) -> Float
    func offset(_ p0: Int32) -> Int64
}

final class Halver: Scaler, Hashable {
    let base: Int64

    init(_ base: Int64) {
        self.base = base
    }

    func half(_ x: Float) -> Float {
        return x / 2.0
    }

    func offset(_ by: Int32) -> Int64 {
        return self.base &+ Int64(truncatingIfNeeded: by)
    }

    static func == (a: Halver, b: Halver) -> Bool {
        [true, a.base == b.base].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(base)
    }
}

func checkArgumentWidthsFollowTheInterface() -> Double {
    let s: any Scaler = Halver(40)
    return off(s.half(9.0), 4.5) + offInt(s.offset(2), 42)
}

func checkInterfaceInAnEither() -> Double {
    let r: Either<any Shape, Int64> = Either<any Shape, Int64>.ok(Rect(4, 4))
    if !r.isOk {
        return 1.0
    }
    let s: any Shape = r.okValue
    return offInt(s.area(), 16)
}

func checkInterfaceCapturedByAClosure() -> Double {
    let s: any Shape = Square(7)
    let f: () -> Int64 = { () in s.area() }
    return offInt(f(), 49)
}

func checkInterfaceThroughAVar() -> Double {
    var s: any Shape = Square(2)
    let first: Int64 = s.area()
    s = Rect(3, 5)
    return offInt(first &* 100 &+ s.area(), 415)
}

protocol Weighted: AnyObject {
    func weight() -> Int64
}

func totalWeight(_ a: any Weighted, _ b: any Weighted) -> Int64 {
    return a.weight() &+ b.weight()
}

func checkGenericStructConvertsToAnInterface() -> Double {
    let a: Cell<Int64> = Cell(5)
    let b: Cell<Double> = Cell(2.5)
    return offInt(totalWeight(a, b), 14) + offInt(a.get(), 5) + off(Float(b.get()), 2.5)
}

func checkGenericInterfaceValuesInADarray() -> Double {
    let all: MutableList<any Weighted> = MutableList()
    let a: Cell<Int64> = Cell(1)
    all.append(a)
    all.append(a)
    return offInt(all[0].weight() &+ all[1].weight(), 14)
}

final class Cell<T: Hashable>: Weighted, Hashable {
    let value: T

    init(_ value: T) {
        self.value = value
    }

    func weight() -> Int64 {
        return 7
    }

    func get() -> T {
        return self.value
    }

    static func == (a: Cell, b: Cell) -> Bool {
        [true, a.value == b.value].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(value)
    }
}
