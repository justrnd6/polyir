final class Point: Hashable {
    let x: Double
    let y: Double

    init(_ x: Double, _ y: Double) {
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

func distanceSquared(_ a: Point, _ b: Point) -> Double {
    let dx: Double = a.x - b.x
    let dy: Double = a.y - b.y
    return dx * dx + dy * dy
}

func distFromOrigin(_ bx: Double, _ by: Double) -> Double {
    return distanceSquared(Point(0.0, 0.0), Point(bx, by))
}
