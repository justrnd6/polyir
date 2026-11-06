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

fun literalEqualsItself(): Long {
    val c: Byte = 65.toByte()
    if (c == 65.toByte()) {
        return 1L
    }
    return 0L
}

fun literalsDistinguishCorrectly(): Long {
    val a: Byte = 97.toByte()
    val b: Byte = 98.toByte()
    if (a == b) {
        return 0L
    }
    return 1L
}

fun gridOfCharsRoundTrip(): Long {
    val g: Grid<Byte> = Grid(listOf(1, 3), 0.toByte())
    g.set(120.toByte(), 0L, 0L)
    g.set(121.toByte(), 0L, 1L)
    g.set(122.toByte(), 0L, 2L)
    if (g.get(0L, 0L) == 120.toByte() && g.get(0L, 1L) == 121.toByte() && g.get(0L, 2L) == 122.toByte()) {
        return 1L
    }
    return 0L
}

fun darrayOfCharsRoundTrip(): Long {
    val cs: MutableList<Byte> = mutableListOf()
    cs.add(104.toByte())
    cs.add(105.toByte())
    if (cs[0] == 104.toByte() && cs[1] == 105.toByte()) {
        return 1L
    }
    return 0L
}
