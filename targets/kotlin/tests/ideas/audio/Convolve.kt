fun convolveSample(i: Int, x: MutableList<Float>, h: MutableList<Float>, p: MutableList<Int>): Float {
    val nx: Int = p[0]
    val nh: Int = p[1]
    var first: Int = i - (nx - 1)
    if (first < 0) {
        first = 0
    }
    var last: Int = i
    if (last > nh - 1) {
        last = nh - 1
    }
    var sum: Float = 0.0f
    run {
        var j: Int = first
        while (j <= last) {
            sum = sum + x[i - j] * h[j]
            j = j + 1
        }
    }
    return sum
}
