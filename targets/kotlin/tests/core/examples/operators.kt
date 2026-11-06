fun bitwisePrecedenceCheck(): Long {
    val a: Long = 6L
    val b: Long = 3L
    val c: Long = 1L
    val d: Long = 2L
    return a or (b and c xor d)
}

fun shiftPrecedenceCheck(): Long {
    val x: Long = 1L
    return x shl 2L.toInt() and 3L
}
