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

fun weightedSum3D(): Long {
    val t: Grid<Long> = Grid(listOf(2, 3, 4), 0L)
    run {
        var i: Long = 0L
        while (i < 2L) {
            run {
                var j: Long = 0L
                while (j < 3L) {
                    run {
                        var k: Long = 0L
                        while (k < 4L) {
                            t.set(i * 100L + j * 10L + k, i, j, k)
                            k = k + 1L
                        }
                    }
                    j = j + 1L
                }
            }
            i = i + 1L
        }
    }
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 2L) {
            run {
                var j: Long = 0L
                while (j < 3L) {
                    run {
                        var k: Long = 0L
                        while (k < 4L) {
                            total = total + t.get(i, j, k) * (i + 1L) * (j + 1L)
                            k = k + 1L
                        }
                    }
                    j = j + 1L
                }
            }
            i = i + 1L
        }
    }
    return total
}

fun cornerRoundTrip(): Long {
    val t: Grid<Long> = Grid(listOf(2, 2, 2), 0L)
    t.set(1L, 0L, 0L, 0L)
    t.set(2L, 0L, 0L, 1L)
    t.set(3L, 0L, 1L, 0L)
    t.set(4L, 1L, 0L, 0L)
    t.set(5L, 1L, 1L, 1L)
    return t.get(1L, 1L, 1L) * 100L + t.get(1L, 0L, 0L) * 10L + t.get(0L, 1L, 0L)
}

fun dimsCheck(): Long {
    val t: Grid<Long> = Grid(listOf(2, 3, 4), 0L)
    return t.dims[0].toLong() * 100L + t.dims[1].toLong() * 10L + t.dims[2].toLong()
}

fun oneDimensional(): Long {
    val t: Grid<Long> = Grid(listOf(4), 0L)
    t.set(10L, 0L)
    t.set(20L, 1L)
    t.set(30L, 2L)
    t.set(40L, 3L)
    return t.get(1L) + t.get(2L)
}

fun fourDimensional(): Long {
    val t: Grid<Long> = Grid(listOf(2, 2, 2, 2), 0L)
    t.set(1L, 0L, 0L, 0L, 0L)
    t.set(99L, 1L, 1L, 1L, 1L)
    t.set(5L, 0L, 1L, 0L, 1L)
    return t.get(1L, 1L, 1L, 1L) * 100L + t.get(0L, 1L, 0L, 1L)
}
