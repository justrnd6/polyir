// A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(value, i, j)`
// writes one.
class Grid<T>(val dims: List<Int>, zero: T) {
    private val cells = MutableList(dims.fold(1) { count, size -> count * size }) { zero }

    private fun place(indices: LongArray): Int = dims.indices.fold(0) { at, i -> at * dims[i] + indices[i].toInt() }

    fun get(vararg indices: Long): T = cells[place(indices)]

    fun set(value: T, vararg indices: Long) {
        cells[place(indices)] = value
    }

    fun copy(): Grid<T> {
        val out = Grid(dims, cells[0])
        for (i in cells.indices) {
            out.cells[i] = cells[i]
        }
        return out
    }
}

fun fillAndWeight(scale: Long): Long {
    val g: Grid<Long> = Grid(listOf(3, 4), 0L)
    run {
        var r: Long = 0L
        while (r < 3L) {
            run {
                var c: Long = 0L
                while (c < 4L) {
                    g.set((r * 4L + c) * scale, r, c)
                    c = c + 1L
                }
            }
            r = r + 1L
        }
    }
    var total: Long = 0L
    run {
        var r: Long = 0L
        while (r < 3L) {
            run {
                var c: Long = 0L
                while (c < 4L) {
                    total = total + g.get(r, c) * (r + 1L)
                    c = c + 1L
                }
            }
            r = r + 1L
        }
    }
    return total
}

fun singleCellRoundTrip(): Long {
    val g: Grid<Long> = Grid(listOf(2, 2), 0L)
    g.set(10L, 0L, 0L)
    g.set(20L, 0L, 1L)
    g.set(30L, 1L, 0L)
    g.set(40L, 1L, 1L)
    return g.get(1L, 0L) - g.get(0L, 1L)
}

fun dimsCheck(): Long {
    val g: Grid<Long> = Grid(listOf(3, 4), 0L)
    return g.dims[0].toLong() * 100L + g.dims[1].toLong()
}
