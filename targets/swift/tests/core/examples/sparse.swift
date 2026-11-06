// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero. `set(value, row, col)` writes one.
final class Sparse<T> {
    private var rows: [Int64] = []
    private var cols: [Int64] = []
    private var values: [T] = []
    private let zero: T

    init(_ zero: T) {
        self.zero = zero
    }

    var count: Int64 {
        Int64(values.count)
    }

    func get(_ row: Int64, _ col: Int64) -> T {
        var found = zero
        for i in values.indices where rows[i] == row && cols[i] == col {
            found = values[i]
        }
        return found
    }

    func set(_ value: T, _ row: Int64, _ col: Int64) {
        rows.append(row)
        cols.append(col)
        values.append(value)
    }

    func copy() -> Sparse<T> {
        let out = Sparse(zero)
        out.rows = rows
        out.cols = cols
        out.values = values
        return out
    }
}

func writeAndReadBack() -> Int64 {
    let sp: Sparse<Int64> = Sparse(0)
    sp.set(10, 0, 0)
    sp.set(20, 2, 3)
    sp.set(30, 5, 1)
    return (sp.get(2, 3) &* 100 &+ sp.get(5, 1) &* 10) &+ sp.get(0, 0)
}

func unsetCellIsZero() -> Int64 {
    let sp: Sparse<Int64> = Sparse(0)
    sp.set(99, 1, 1)
    return sp.get(9, 9)
}

func lastWriteWins() -> Int64 {
    let sp: Sparse<Int64> = Sparse(0)
    sp.set(1, 3, 3)
    sp.set(2, 3, 3)
    sp.set(3, 3, 3)
    return sp.get(3, 3)
}

func oneDimensionalRoundTrip() -> Int64 {
    let sp: Sparse<Int64> = Sparse(0)
    sp.set(100, 0, 0)
    sp.set(200, 7, 0)
    sp.set(300, 3, 0)
    return (sp.get(7, 0) &* 100 &+ sp.get(3, 0) &* 10) &+ sp.get(0, 0)
}

func lenAfterWrites() -> Int64 {
    let sp: Sparse<Int64> = Sparse(0)
    sp.set(1, 0, 0)
    sp.set(2, 1, 1)
    sp.set(3, 1, 1)
    return sp.count
}
