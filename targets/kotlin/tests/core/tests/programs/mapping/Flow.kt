import kotlin.system.exitProcess

fun sumTo(n: Long): Long {
    var label: Long = 0L
    var total: Long = 0L
    var i: Long = 0L
    var result: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                total = 0L
                i = 1L
                label = 1L
                continue@machine
            }
            1L -> {
                if (i > n) {
                    result = total
                    label = 3L
                    continue@machine
                } else {
                    label = 2L
                    continue@machine
                }
            }
            2L -> {
                total = total + i
                i = i + 1L
                label = 1L
                continue@machine
            }
            3L -> {
                return result
            }
        }
    }
}

fun gcd(a0: Long, b0: Long): Long {
    var label: Long = 0L
    var a: Long = 0L
    var b: Long = 0L
    var result: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                a = a0
                b = b0
                label = 1L
                continue@machine
            }
            1L -> {
                if (b == 0L) {
                    result = a
                    label = 3L
                    continue@machine
                } else {
                    label = 2L
                    continue@machine
                }
            }
            2L -> {
                val r: Long = a % b
                a = b
                b = r
                label = 1L
                continue@machine
            }
            3L -> {
                return result
            }
        }
    }
}

fun collatz(n: Long): Long {
    var label: Long = 0L
    var x: Long = 0L
    var steps: Long = 0L
    var count: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                x = n
                steps = 0L
                label = 1L
                continue@machine
            }
            1L -> {
                if (x == 1L) {
                    count = steps
                    label = 5L
                    continue@machine
                } else {
                    label = 2L
                    continue@machine
                }
            }
            2L -> {
                if (x % 2L == 0L) {
                    label = 3L
                    continue@machine
                } else {
                    label = 4L
                    continue@machine
                }
            }
            3L -> {
                x = x / 2L
                steps = steps + 1L
                label = 1L
                continue@machine
            }
            4L -> {
                x = 3L * x + 1L
                steps = steps + 1L
                label = 1L
                continue@machine
            }
            5L -> {
                return count
            }
        }
    }
}

fun swapped(a: Long, b: Long): Long {
    var label: Long = 0L
    var x: Long = 0L
    var y: Long = 0L
    var k: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                x = a
                y = b
                k = 0L
                label = 1L
                continue@machine
            }
            1L -> {
                if (k == 3L) {
                    label = 3L
                    continue@machine
                } else {
                    label = 2L
                    continue@machine
                }
            }
            2L -> {
                var next_x: Long = y
                var next_y: Long = x
                var next_k: Long = k + 1L
                x = next_x
                y = next_y
                k = next_k
                label = 1L
                continue@machine
            }
            3L -> {
                return x * 10L + y
            }
        }
    }
}

fun weight(day: Long): Long {
    var label: Long = 0L
    var w: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                if (day == 0L) {
                    label = 1L
                    continue@machine
                } else if (day == 6L) {
                    label = 1L
                    continue@machine
                } else if (day == 1L) {
                    label = 2L
                    continue@machine
                } else if (day == 2L) {
                    label = 2L
                    continue@machine
                } else {
                    label = 3L
                    continue@machine
                }
            }
            1L -> {
                w = 1L
                label = 4L
                continue@machine
            }
            2L -> {
                w = 2L
                label = 4L
                continue@machine
            }
            3L -> {
                w = 3L
                label = 4L
                continue@machine
            }
            4L -> {
                return w * 100L
            }
        }
    }
}

fun zigzag(a: Long): Long {
    var label: Long = 0L
    var p: Long = 0L
    var q: Long = 0L
    var r: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                if (a == 0L) {
                    p = a
                    label = 1L
                    continue@machine
                } else {
                    q = a
                    label = 2L
                    continue@machine
                }
            }
            1L -> {
                val next: Long = p + 1L
                if (next > 5L) {
                    r = next
                    label = 3L
                    continue@machine
                } else {
                    q = next
                    label = 2L
                    continue@machine
                }
            }
            2L -> {
                if (q > 5L) {
                    r = q
                    label = 3L
                    continue@machine
                } else {
                    p = q + 1L
                    label = 1L
                    continue@machine
                }
            }
            3L -> {
                return r * 2L
            }
        }
    }
}

fun main(): Long {
    var score: Long = sumTo(10L)
    score = score + gcd(48L, 18L)
    score = score + collatz(6L)
    score = score + swapped(4L, 7L)
    score = score + weight(0L) + weight(2L) + weight(9L)
    score = score + zigzag(0L) + zigzag(3L)
    return score
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
