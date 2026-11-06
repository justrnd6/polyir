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

func fillAndWeight(_ scale: Int64) -> Int64 {
    let g: Grid<Int64> = Grid([3, 4], 0)
    do {
        var r: Int64 = 0
        while r < 3 {
            do {
                var c: Int64 = 0
                while c < 4 {
                    g.set((r &* 4 &+ c) &* scale, r, c)
                    c = c &+ 1
                }
            }
            r = r &+ 1
        }
    }
    var total: Int64 = 0
    do {
        var r: Int64 = 0
        while r < 3 {
            do {
                var c: Int64 = 0
                while c < 4 {
                    total = total &+ g.get(r, c) &* (r &+ 1)
                    c = c &+ 1
                }
            }
            r = r &+ 1
        }
    }
    return total
}

func singleCellRoundTrip() -> Int64 {
    let g: Grid<Int64> = Grid([2, 2], 0)
    g.set(10, 0, 0)
    g.set(20, 0, 1)
    g.set(30, 1, 0)
    g.set(40, 1, 1)
    return g.get(1, 0) &- g.get(0, 1)
}

func dimsCheck() -> Int64 {
    let g: Grid<Int64> = Grid([3, 4], 0)
    return Int64(g.dims[0]) &* 100 &+ Int64(g.dims[1])
}
