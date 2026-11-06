fun drawStems(pic: MutableList<Float>, w: Long, h: Long, values: MutableList<Float>, n: Long, low: Float, high: Float): Long {
    run {
        var k: Long = 0L
        while (k < w * h) {
            pic[k.toInt()] = 0.96f
            k = k + 1L
        }
    }
    val zero: Long = (high / (high - low) * (h.toFloat())).toLong()
    run {
        var x: Long = 0L
        while (x < w) {
            pic[(zero * w + x).toInt()] = 0.55f
            x = x + 1L
        }
    }
    run {
        var j: Long = 0L
        while (j < n) {
            val column: Long = (((j.toFloat()) + 0.5f) * (w.toFloat()) / (n.toFloat())).toLong()
            var top: Long = ((high - values[j.toInt()]) / (high - low) * ((h - 1L).toFloat())).toLong()
            if (top < 0L) {
                top = 0L
            }
            if (top > h - 1L) {
                top = h - 1L
            }
            val from: Long = minOf(top, zero)
            val to: Long = maxOf(top, zero)
            run {
                var y: Long = from
                while (y <= to) {
                    pic[(y * w + column).toInt()] = 0.1f
                    y = y + 1L
                }
            }
            run {
                var dy: Long = 0L - 1L
                while (dy <= 1L) {
                    run {
                        var dx: Long = 0L - 1L
                        while (dx <= 1L) {
                            val yy: Long = top + dy
                            val xx: Long = column + dx
                            if (yy >= 0L && yy < h && xx >= 0L && xx < w) {
                                pic[(yy * w + xx).toInt()] = 0.1f
                            }
                            dx = dx + 1L
                        }
                    }
                    dy = dy + 1L
                }
            }
            j = j + 1L
        }
    }
    return 0L
}

fun minOf(a: Long, b: Long): Long {
    if (a < b) {
        return a
    }
    return b
}

fun maxOf(a: Long, b: Long): Long {
    if (a > b) {
        return a
    }
    return b
}

fun drawWave(pic: MutableList<Float>, w: Long, h: Long, values: MutableList<Float>, n: Long, low: Float, high: Float): Long {
    run {
        var k: Long = 0L
        while (k < w * h) {
            pic[k.toInt()] = 0.96f
            k = k + 1L
        }
    }
    val zero: Long = (high / (high - low) * (h.toFloat())).toLong()
    run {
        var x: Long = 0L
        while (x < w) {
            pic[(zero * w + x).toInt()] = 0.55f
            x = x + 1L
        }
    }
    run {
        var x: Long = 0L
        while (x < w) {
            val from: Long = x * n / w
            var upto: Long = (x + 1L) * n / w
            if (upto <= from) {
                upto = from + 1L
            }
            var lowest: Float = values[from.toInt()]
            var highest: Float = values[from.toInt()]
            run {
                var j: Long = from
                while (j < upto) {
                    if (values[j.toInt()] < lowest) {
                        lowest = values[j.toInt()]
                    }
                    if (values[j.toInt()] > highest) {
                        highest = values[j.toInt()]
                    }
                    j = j + 1L
                }
            }
            var top: Long = ((high - highest) / (high - low) * ((h - 1L).toFloat())).toLong()
            var bottom: Long = ((high - lowest) / (high - low) * ((h - 1L).toFloat())).toLong()
            if (top < 0L) {
                top = 0L
            }
            if (bottom > h - 1L) {
                bottom = h - 1L
            }
            run {
                var y: Long = top
                while (y <= bottom) {
                    pic[(y * w + x).toInt()] = 0.1f
                    y = y + 1L
                }
            }
            x = x + 1L
        }
    }
    return 0L
}

fun drawCurve(pic: MutableList<Float>, w: Long, h: Long, values: MutableList<Float>, n: Long, low: Float, high: Float): Long {
    run {
        var k: Long = 0L
        while (k < w * h) {
            pic[k.toInt()] = 0.96f
            k = k + 1L
        }
    }
    val zero: Long = (high / (high - low) * (h.toFloat())).toLong()
    run {
        var x: Long = 0L
        while (x < w) {
            pic[(zero * w + x).toInt()] = 0.55f
            x = x + 1L
        }
    }
    var previous: Long = curveRow(values[0], h, low, high)
    run {
        var x: Long = 0L
        while (x < w) {
            val row: Long = curveRow(values[(x * n / w).toInt()], h, low, high)
            run {
                var y: Long = minOf(row, previous)
                while (y <= maxOf(row, previous)) {
                    pic[(y * w + x).toInt()] = 0.1f
                    y = y + 1L
                }
            }
            previous = row
            x = x + 1L
        }
    }
    return 0L
}

fun curveRow(value: Float, h: Long, low: Float, high: Float): Long {
    var row: Long = ((high - value) / (high - low) * ((h - 1L).toFloat())).toLong()
    if (row < 0L) {
        row = 0L
    }
    if (row > h - 1L) {
        row = h - 1L
    }
    return row
}
