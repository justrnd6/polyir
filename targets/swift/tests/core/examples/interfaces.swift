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

protocol Shape: AnyObject {
    func area() -> Int64
    func scaled(_ p0: Int64) -> Int64
}

protocol Named: AnyObject {
    func id() -> Int64
}

protocol Combiner: AnyObject {
    func combine(_ p0: any Shape) -> Int64
}

final class Square: Shape, Named, Hashable {
    let side: Int64

    init(_ side: Int64) {
        self.side = side
    }

    func area() -> Int64 {
        return self.side &* self.side
    }

    func scaled(_ k: Int64) -> Int64 {
        return self.side &* k
    }

    func id() -> Int64 {
        return 1
    }

    static func == (a: Square, b: Square) -> Bool {
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

    func scaled(_ k: Int64) -> Int64 {
        return self.w &* k
    }

    static func == (a: Rect, b: Rect) -> Bool {
        [true, a.w == b.w, a.h == b.h].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(w)
        hasher.combine(h)
    }
}

final class Summer: Combiner, Hashable {
    let base: Int64

    init(_ base: Int64) {
        self.base = base
    }

    func combine(_ other: any Shape) -> Int64 {
        return self.base &+ other.area()
    }

    static func == (a: Summer, b: Summer) -> Bool {
        [true, a.base == b.base].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(base)
    }
}

final class Holder: Hashable {
    let shape: any Shape
    let tag: Int64

    init(_ shape: any Shape, _ tag: Int64) {
        self.shape = shape
        self.tag = tag
    }

    static func == (a: Holder, b: Holder) -> Bool {
        [true, a.shape === b.shape, a.tag == b.tag].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(ObjectIdentifier(shape))
        hasher.combine(tag)
    }
}

func totalArea(_ s: any Shape) -> Int64 {
    return s.area()
}

func directCall() -> Int64 {
    return totalArea(Square(3)) &* 100 &+ totalArea(Rect(4, 5))
}

func pick(_ which: Int64) -> any Shape {
    if which == 0 {
        return Square(6)
    }
    return Rect(2, 7)
}

func chosen() -> Int64 {
    let a: any Shape = pick(0)
    let b: any Shape = pick(1)
    return (a.area() &* 1000 &+ b.area()) &+ b.scaled(10)
}

func sumAll() -> Int64 {
    let shapes: MutableList<any Shape> = MutableList()
    shapes.append(Square(2))
    shapes.append(Rect(3, 4))
    shapes.append(Square(5))
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < shapes.count {
            total = total &+ shapes[i].area()
            i = i &+ 1
        }
    }
    return total
}

func viaField() -> Int64 {
    let h: Holder = Holder(Rect(5, 6), 7)
    return h.shape.area() &* 10 &+ h.tag
}

func twoInterfaces() -> Int64 {
    let sq: Square = Square(4)
    let n: any Named = sq
    let s: any Shape = sq
    return n.id() &* 1000 &+ s.area()
}

func interfaceArgument() -> Int64 {
    let c: any Combiner = Summer(100)
    return c.combine(Rect(3, 3))
}

func main() -> Int64 {
    return ((((directCall() &+ chosen()) &+ sumAll()) &+ viaField()) &+ twoInterfaces()) &+ interfaceArgument()
}

exit(Int32(truncatingIfNeeded: main()))
