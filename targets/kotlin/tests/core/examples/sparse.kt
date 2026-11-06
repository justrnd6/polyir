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

fun writeAndReadBack(): Long {
    val sp: Sparse<Long> = Sparse(0L)
    sp.set(10L, 0L, 0L)
    sp.set(20L, 2L, 3L)
    sp.set(30L, 5L, 1L)
    return sp.get(2L, 3L) * 100L + sp.get(5L, 1L) * 10L + sp.get(0L, 0L)
}

fun unsetCellIsZero(): Long {
    val sp: Sparse<Long> = Sparse(0L)
    sp.set(99L, 1L, 1L)
    return sp.get(9L, 9L)
}

fun lastWriteWins(): Long {
    val sp: Sparse<Long> = Sparse(0L)
    sp.set(1L, 3L, 3L)
    sp.set(2L, 3L, 3L)
    sp.set(3L, 3L, 3L)
    return sp.get(3L, 3L)
}

fun oneDimensionalRoundTrip(): Long {
    val sp: Sparse<Long> = Sparse(0L)
    sp.set(100L, 0L, 0L)
    sp.set(200L, 7L, 0L)
    sp.set(300L, 3L, 0L)
    return sp.get(7L, 0L) * 100L + sp.get(3L, 0L) * 10L + sp.get(0L, 0L)
}

fun lenAfterWrites(): Long {
    val sp: Sparse<Long> = Sparse(0L)
    sp.set(1L, 0L, 0L)
    sp.set(2L, 1L, 1L)
    sp.set(3L, 1L, 1L)
    return sp.size.toLong()
}
