fun square(x: Long): Long {
    return x * x
}

fun sub(a: Long, b: Long): Long {
    return a - b
}

fun equivalenceCheck(x: Long): Long {
    return square(x) - square(x)
}

fun orderingCheck(a: Long, b: Long): Long {
    return sub(a, b) - (a - b)
}

fun pushViaMethodPersists(): Long {
    val arr: MutableList<Long> = mutableListOf()
    arr.add(10L)
    arr.add(20L)
    return arr.size.toLong()
}

fun pushViaMethodSum(): Long {
    val arr: MutableList<Long> = mutableListOf()
    arr.add(1L)
    arr.add(2L)
    arr.add(3L)
    return arr[0] + arr[1] + arr[2]
}
