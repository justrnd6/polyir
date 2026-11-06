fun sumBuf(buf: MutableList<Double>, len: Long): Double {
    var i: Long = 0L
    var total: Double = 0.0
    while (i < len) {
        i = i + 1L
        total = total + buf[i.toInt()]
    }
    return total
}
