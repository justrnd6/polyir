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
