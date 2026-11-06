final class Rect: Hashable {
    let width: Int64
    let height: Int64

    init(_ width: Int64, _ height: Int64) {
        self.width = width
        self.height = height
    }

    static func == (a: Rect, b: Rect) -> Bool {
        [true, a.width == b.width, a.height == b.height].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(width)
        hasher.combine(height)
    }
}

func area(_ r: Rect) -> Int64 {
    return r.width &* r.height
}
