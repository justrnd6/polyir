// An operator lane by lane on two vectors.
private fun <T> lanes(a: List<T>, b: List<T>, op: (T, T) -> T): MutableList<T> = a.indices.map { op(a[it], b[it]) }.toMutableList()

fun vecMulSum(a: Double, b: Double): Double {
    val v1: MutableList<Double> = mutableListOf(a, a + 1.0, a + 2.0, a + 3.0)
    val v2: MutableList<Double> = mutableListOf(b, b, b, b)
    val p: MutableList<Double> = lanes(v1, v2) { x, y -> x * y }
    return p[0] + p[1] + p[2] + p[3]
}

fun sumI64Vec(a: Long, b: Long, c: Long): Long {
    val v: MutableList<Long> = mutableListOf(a, b, c)
    return v[0] + v[1] + v[2]
}

fun vecMulSumViaVar(a: Double, b: Double): Double {
    val v1: MutableList<Double> = mutableListOf(a, a + 1.0, a + 2.0, a + 3.0)
    val v2: MutableList<Double> = mutableListOf(b, b, b, b)
    val p: MutableList<Double> = lanes(v1, v2) { x, y -> x * y }
    var total: Double = 0.0
    run {
        var i: Long = 0L
        while (i < 4L) {
            total = total + p[i.toInt()]
            i = i + 1L
        }
    }
    return total
}
