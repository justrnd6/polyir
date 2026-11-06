fun histTest(): Long {
    val hist: MutableList<Long> = MutableList(4) { 0L }
    hist[0] = 10L
    hist[1] = 20L
    hist[2] = 30L
    hist[3] = 40L
    return hist[1] + hist[2]
}

fun lenCheck(): Long {
    val hist: MutableList<Long> = MutableList(4) { 0L }
    return hist.size.toLong()
}

fun sumFixed(arr: MutableList<Long>): Long {
    return arr[0] + arr[1] + arr[2] + arr[3]
}

fun lenOnParam(arr: MutableList<Long>): Long {
    return arr.size.toLong()
}

fun callWithLocal(): Long {
    val hist: MutableList<Long> = MutableList(4) { 0L }
    hist[0] = 1L
    hist[1] = 2L
    hist[2] = 3L
    hist[3] = 4L
    return sumFixed(hist) * 100L + lenOnParam(hist)
}
