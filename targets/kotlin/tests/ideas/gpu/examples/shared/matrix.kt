fun transform(p: MutableList<Float>, m: MutableList<Float>): MutableList<Float> {
    return mutableListOf(m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3] * p[3], m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7] * p[3], m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11] * p[3], m[12] * p[0] + m[13] * p[1] + m[14] * p[2] + m[15] * p[3])
}

fun identity(m: MutableList<Float>): Long {
    run {
        var k: Long = 0L
        while (k < 16L) {
            m[k.toInt()] = 0.0f
            k = k + 1L
        }
    }
    m[0] = 1.0f
    m[5] = 1.0f
    m[10] = 1.0f
    m[15] = 1.0f
    return 0L
}

fun multiply4(a: MutableList<Float>, b: MutableList<Float>, out: MutableList<Float>): Long {
    run {
        var row: Long = 0L
        while (row < 4L) {
            run {
                var col: Long = 0L
                while (col < 4L) {
                    var sum: Float = 0.0f
                    run {
                        var k: Long = 0L
                        while (k < 4L) {
                            sum = sum + a[(row * 4L + k).toInt()] * b[(k * 4L + col).toInt()]
                            k = k + 1L
                        }
                    }
                    out[(row * 4L + col).toInt()] = sum
                    col = col + 1L
                }
            }
            row = row + 1L
        }
    }
    return 0L
}
