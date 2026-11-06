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
