// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero. `set(value, row, col)` writes one.
class Sparse<T>(private val zero: T) {
    private val rows = mutableListOf<Long>()
    private val cols = mutableListOf<Long>()
    private val values = mutableListOf<T>()

    val size: Int get() = values.size

    fun get(row: Long, col: Long): T {
        var found = zero
        for (i in values.indices) {
            if (rows[i] == row && cols[i] == col) {
                found = values[i]
            }
        }
        return found
    }

    fun set(value: T, row: Long, col: Long) {
        rows.add(row)
        cols.add(col)
        values.add(value)
    }

    fun copy(): Sparse<T> {
        val out = Sparse(zero)
        out.rows.addAll(rows)
        out.cols.addAll(cols)
        out.values.addAll(values)
        return out
    }
}
