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

func relu(_ x: Double) -> Double {
    if x < 0.0 {
        return 0.0
    }
    return x
}

func xorForward(_ x1: Double, _ x2: Double) -> Double {
    let w1: Grid<Double> = Grid([2, 2], 0.0)
    w1.set(1.0, 0, 0)
    w1.set(1.0, 0, 1)
    w1.set(1.0, 1, 0)
    w1.set(1.0, 1, 1)
    let b1: Grid<Double> = Grid([2], 0.0)
    b1.set(0.0, 0)
    b1.set(0.0 - 1.0, 1)
    let hidden: Grid<Double> = Grid([2], 0.0)
    do {
        var i: Int64 = 0
        while i < 2 {
            var preActivation: Double = b1.get(i)
            preActivation = preActivation + w1.get(i, 0) * x1
            preActivation = preActivation + w1.get(i, 1) * x2
            hidden.set(relu(preActivation), i)
            i = i &+ 1
        }
    }
    let w2: Grid<Double> = Grid([2], 0.0)
    w2.set(1.0, 0)
    w2.set(0.0 - 2.0, 1)
    let b2: Double = 0.0
    var output: Double = b2
    do {
        var i: Int64 = 0
        while i < 2 {
            output = output + w2.get(i) * hidden.get(i)
            i = i &+ 1
        }
    }
    return output
}

func xorPredict(_ x1: Double, _ x2: Double) -> Int64 {
    if xorForward(x1, x2) >= 0.5 {
        return 1
    }
    return 0
}
