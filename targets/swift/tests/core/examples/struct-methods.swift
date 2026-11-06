final class Point: Hashable {
    let x: Double
    let y: Double

    init(_ x: Double, _ y: Double) {
        self.x = x
        self.y = y
    }

    func dist() -> Double {
        return self.x * self.x + self.y * self.y
    }

    static func == (a: Point, b: Point) -> Bool {
        [true, a.x == b.x, a.y == b.y].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(x)
        hasher.combine(y)
    }
}

final class Circle: Hashable {
    let r: Double

    init(_ r: Double) {
        self.r = r
    }

    func dist() -> Double {
        return self.r * self.r
    }

    static func == (a: Circle, b: Circle) -> Bool {
        [true, a.r == b.r].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(r)
    }
}

func distSum(_ px: Double, _ py: Double, _ r: Double) -> Double {
    let p: Point = Point(px, py)
    let c: Circle = Circle(r)
    return p.dist() + c.dist()
}

final class Pair: Hashable {
    let a: Int64
    let b: Int64

    init(_ a: Int64, _ b: Int64) {
        self.a = a
        self.b = b
    }

    func sum() -> Int64 {
        return self.a &+ self.b
    }

    func doubledSum() -> Int64 {
        return self.sum() &* 2
    }

    static func == (a: Pair, b: Pair) -> Bool {
        [true, a.a == b.a, a.b == b.b].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(a)
        hasher.combine(b)
    }
}

func doubledSumCheck(_ a: Int64, _ b: Int64) -> Int64 {
    let p: Pair = Pair(a, b)
    return p.doubledSum()
}
