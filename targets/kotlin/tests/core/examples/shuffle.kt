fun reverse4(a: Long, b: Long, c: Long): Long {
    val v: MutableList<Long> = mutableListOf(a, b, c, 40L)
    val r: MutableList<Long> = listOf(3, 2, 1, 0).map { v[it] }.toMutableList()
    return r[0] * 1000L + r[1] * 100L + r[2] * 10L + r[3]
}

fun duplicateFirstTwo(a: Long, b: Long, c: Long): Long {
    val v: MutableList<Long> = mutableListOf(a, b, c, 4L)
    val r: MutableList<Long> = listOf(0, 0, 1, 1).map { v[it] }.toMutableList()
    return r[0] * 1000L + r[1] * 100L + r[2] * 10L + r[3]
}
