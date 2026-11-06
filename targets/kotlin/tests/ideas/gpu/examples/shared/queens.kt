fun queens(t: Int, prefixes: MutableList<Int>, p: MutableList<Int>, stack: MutableList<Int>): Int {
    val n: Int = p[0]
    val depth: Int = p[1]
    val full: Int = ((1L shl (n.toLong()).toInt()) - 1L).toInt()
    val base: Int = t * (n + 1) * 4
    stack[base] = prefixes[t * 3]
    stack[base + 1] = prefixes[t * 3 + 1]
    stack[base + 2] = prefixes[t * 3 + 2]
    stack[base + 3] = full and (prefixes[t * 3] or prefixes[t * 3 + 1] or prefixes[t * 3 + 2]).inv()
    var level: Int = 0
    var count: Int = 0
    run {
        var step: Long = 0L
        while (level >= 0) {
            val at: Int = base + level * 4
            val free: Int = stack[at + 3]
            if (free == 0) {
                level = level - 1
            } else {
                val bit: Int = free and ((0L - (free.toLong())).toInt())
                stack[at + 3] = free xor bit
                if (depth + level + 1 == n) {
                    count = count + 1
                } else {
                    val nextCols: Int = stack[at] or bit
                    val nextLeft: Int = stack[at + 1] or bit shl 1.toInt() and full
                    val nextRight: Int = stack[at + 2] or bit shr 1.toInt()
                    level = level + 1
                    val below: Int = base + level * 4
                    stack[below] = nextCols
                    stack[below + 1] = nextLeft
                    stack[below + 2] = nextRight
                    stack[below + 3] = full and (nextCols or nextLeft or nextRight).inv()
                }
            }
            step = step + 1L
        }
    }
    return count
}

fun listPrefixes(n: Long, depth: Long, out: MutableList<Int>): Long {
    var total: Long = 1L
    var r: Long = 0L
    while (r < depth) {
        total = total * n
        r = r + 1L
    }
    val full: Int = ((1L shl n.toInt()) - 1L).toInt()
    var found: Long = 0L
    run {
        var code: Long = 0L
        while (code < total) {
            var left: Int = 0
            var right: Int = 0
            var cols: Int = 0
            var ok: Long = 1L
            var rest: Long = code
            run {
                var r: Long = 0L
                while (r < depth) {
                    val column: Long = rest % n
                    rest = rest / n
                    val bit: Int = (1L shl column.toInt()).toInt()
                    if (bit and (cols or left or right) != 0) {
                        ok = 0L
                    }
                    cols = cols or bit
                    left = left or bit shl 1.toInt() and full
                    right = right or bit shr 1.toInt()
                    r = r + 1L
                }
            }
            if (ok == 1L) {
                out[(found * 3L).toInt()] = cols
                out[(found * 3L + 1L).toInt()] = left
                out[(found * 3L + 2L).toInt()] = right
                found = found + 1L
            }
            code = code + 1L
        }
    }
    return found
}
