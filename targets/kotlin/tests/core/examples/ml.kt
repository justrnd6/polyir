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

fun relu(x: Double): Double {
    if (x < 0.0) {
        return 0.0
    }
    return x
}

fun xorForward(x1: Double, x2: Double): Double {
    val w1: Grid<Double> = Grid(listOf(2, 2), 0.0)
    w1.set(1.0, 0L, 0L)
    w1.set(1.0, 0L, 1L)
    w1.set(1.0, 1L, 0L)
    w1.set(1.0, 1L, 1L)
    val b1: Grid<Double> = Grid(listOf(2), 0.0)
    b1.set(0.0, 0L)
    b1.set(0.0 - 1.0, 1L)
    val hidden: Grid<Double> = Grid(listOf(2), 0.0)
    run {
        var i: Long = 0L
        while (i < 2L) {
            var preActivation: Double = b1.get(i)
            preActivation = preActivation + w1.get(i, 0L) * x1
            preActivation = preActivation + w1.get(i, 1L) * x2
            hidden.set(relu(preActivation), i)
            i = i + 1L
        }
    }
    val w2: Grid<Double> = Grid(listOf(2), 0.0)
    w2.set(1.0, 0L)
    w2.set(0.0 - 2.0, 1L)
    val b2: Double = 0.0
    var output: Double = b2
    run {
        var i: Long = 0L
        while (i < 2L) {
            output = output + w2.get(i) * hidden.get(i)
            i = i + 1L
        }
    }
    return output
}

fun xorPredict(x1: Double, x2: Double): Long {
    if (xorForward(x1, x2) >= 0.5) {
        return 1L
    }
    return 0L
}
