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

func weightedSum3D() -> Int64 {
    let t: Grid<Int64> = Grid([2, 3, 4], 0)
    do {
        var i: Int64 = 0
        while i < 2 {
            do {
                var j: Int64 = 0
                while j < 3 {
                    do {
                        var k: Int64 = 0
                        while k < 4 {
                            t.set((i &* 100 &+ j &* 10) &+ k, i, j, k)
                            k = k &+ 1
                        }
                    }
                    j = j &+ 1
                }
            }
            i = i &+ 1
        }
    }
    var total: Int64 = 0
    do {
        var i: Int64 = 0
        while i < 2 {
            do {
                var j: Int64 = 0
                while j < 3 {
                    do {
                        var k: Int64 = 0
                        while k < 4 {
                            total = total &+ t.get(i, j, k) &* (i &+ 1) &* (j &+ 1)
                            k = k &+ 1
                        }
                    }
                    j = j &+ 1
                }
            }
            i = i &+ 1
        }
    }
    return total
}

func cornerRoundTrip() -> Int64 {
    let t: Grid<Int64> = Grid([2, 2, 2], 0)
    t.set(1, 0, 0, 0)
    t.set(2, 0, 0, 1)
    t.set(3, 0, 1, 0)
    t.set(4, 1, 0, 0)
    t.set(5, 1, 1, 1)
    return (t.get(1, 1, 1) &* 100 &+ t.get(1, 0, 0) &* 10) &+ t.get(0, 1, 0)
}

func dimsCheck() -> Int64 {
    let t: Grid<Int64> = Grid([2, 3, 4], 0)
    return (Int64(t.dims[Int(0)]) &* 100 &+ Int64(t.dims[Int(1)]) &* 10) &+ Int64(t.dims[Int(2)])
}

func oneDimensional() -> Int64 {
    let t: Grid<Int64> = Grid([4], 0)
    t.set(10, 0)
    t.set(20, 1)
    t.set(30, 2)
    t.set(40, 3)
    return t.get(1) &+ t.get(2)
}

func fourDimensional() -> Int64 {
    let t: Grid<Int64> = Grid([2, 2, 2, 2], 0)
    t.set(1, 0, 0, 0, 0)
    t.set(99, 1, 1, 1, 1)
    t.set(5, 0, 1, 0, 1)
    return t.get(1, 1, 1, 1) &* 100 &+ t.get(0, 1, 0, 1)
}
