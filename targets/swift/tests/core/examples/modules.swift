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

enum Geometry {
    final class Point: Hashable {
        let x: Int64
        let y: Int64

        init(_ x: Int64, _ y: Int64) {
            self.x = x
            self.y = y
        }

        func manhattan() -> Int64 {
            return self.absolute(self.x) &+ self.absolute(self.y)
        }

        func absolute(_ n: Int64) -> Int64 {
            return n < 0 ? 0 &- n : n
        }

        static func == (a: Point, b: Point) -> Bool {
            [true, a.x == b.x, a.y == b.y].allSatisfy { $0 }
        }

        func hash(into hasher: inout Hasher) {
            hasher.combine(x)
            hasher.combine(y)
        }
    }

    static func clamp(_ n: Int64, _ low: Int64, _ high: Int64) -> Int64 {
        return n < low ? low : n > high ? high : n
    }

    static func distance(_ a: Point, _ b: Point) -> Int64 {
        let step: Point = Point(a.x &- b.x, a.y &- b.y)
        return clamp(step.manhattan(), 0, 100)
    }

    enum Shapes {
        protocol Shape: AnyObject {
            func area() -> Int64
        }

        final class Square: Shape, Hashable {
            let side: Int64

            init(_ side: Int64) {
                self.side = side
            }

            func area() -> Int64 {
                return self.side &* self.side
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

            static func == (a: Rect, b: Rect) -> Bool {
                [true, a.w == b.w, a.h == b.h].allSatisfy { $0 }
            }

            func hash(into hasher: inout Hasher) {
                hasher.combine(w)
                hasher.combine(h)
            }
        }

        static func total(_ shapes: MutableList<any Shape>) -> Int64 {
            var sum: Int64 = 0
            for s in shapes {
                sum = sum &+ s.area()
            }
            return sum
        }
    }
}

func main() -> Void {
    let a: Geometry.Point = Geometry.Point(1, 2)
    let b: Geometry.Point = Geometry.Point(4, 0 &- 2)
    let shapes: MutableList<any Geometry.Shapes.Shape> = MutableList([Geometry.Shapes.Square(3), Geometry.Shapes.Rect(2, 5)])
    let size: String = Geometry.Shapes.total(shapes) > 10 ? "big" : "small"
    print("\(Geometry.distance(a, b)) \(size) \(larger(3, 7, false)) \(larger("x", "y", true)) \(firstOf(MutableList(["first", "second"])))")
}

func larger<T>(_ a: T, _ b: T, _ aFirst: Bool) -> T {
    return aFirst ? a : b
}

func firstOf<T>(_ xs: MutableList<T>) -> T {
    return xs[0]
}

main()
