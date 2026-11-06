fun atomicAdd(counts: MutableList<Int>, slot: Int, amount: Int): Int {
    val old: Int = counts[slot]
    counts[slot] = old + amount
    return old
}

fun atomicAddFloat(sums: MutableList<Float>, slot: Int, amount: Float): Float {
    val old: Float = sums[slot]
    sums[slot] = old + amount
    return old
}

fun atomicMin(values: MutableList<Int>, slot: Int, candidate: Int): Int {
    val old: Int = values[slot]
    if (candidate < old) {
        values[slot] = candidate
    }
    return old
}

fun atomicMax(values: MutableList<Int>, slot: Int, candidate: Int): Int {
    val old: Int = values[slot]
    if (candidate > old) {
        values[slot] = candidate
    }
    return old
}

fun groupSize(): Int {
    return 1
}

fun localIndex(i: Int): Int {
    return 0
}

fun groupIndex(i: Int): Int {
    return i
}

fun barrier(): Int {
    return 0
}
