fun fillToCapacity(): Long {
    val arr: MutableList<Long> = MutableList(4) { 0L }
    arr[0] = 10L
    arr[1] = 20L
    arr[2] = 30L
    arr[3] = 40L
    return arr[0] + arr[1] + arr[2] + arr[3]
}

fun capacityCheck(): Long {
    val arr: MutableList<Long> = MutableList(4) { 0L }
    arr[0] = 1L
    arr[1] = 2L
    return arr.size.toLong()
}

fun letBoundIndexedFill(): Long {
    val arr: MutableList<Long> = MutableList(3) { 0L }
    arr[0] = 5L
    arr[1] = 6L
    arr[2] = 7L
    return arr[0] + arr[1] + arr[2]
}

fun sumViaLoop(n: Long): Long {
    val arr: MutableList<Long> = MutableList(10) { 0L }
    run {
        var i: Long = 0L
        while (i < n) {
            arr[i.toInt()] = i
            i = i + 1L
        }
    }
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < n) {
            total = total + arr[i.toInt()]
            i = i + 1L
        }
    }
    return total
}

fun stringElements(): Long {
    val names: MutableList<String> = MutableList(2) { "" }
    names[0] = "first"
    names[1] = "second"
    if (names[0] != "first") {
        return 0L
    }
    if (names[1] != "second") {
        return 0L
    }
    return 1L
}

fun sumParam(arr: MutableList<Long>): Long {
    return arr[0] + arr[1] + arr[2] + arr.size.toLong()
}

fun viaParam(): Long {
    val arr: MutableList<Long> = MutableList(3) { 0L }
    arr[0] = 100L
    arr[1] = 200L
    arr[2] = 300L
    return sumParam(arr)
}
