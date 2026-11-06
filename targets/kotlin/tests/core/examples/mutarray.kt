// A push used as a value gives the list, as in polyir (Kotlin's add gives whether it changed the list).
private fun <T> pushed(items: MutableList<T>, item: T): MutableList<T> {
    items.add(item)
    return items
}

fun sumPushed(a: Long, b: Long, c: Long): Long {
    val arr: MutableList<Long> = mutableListOf()
    arr.add(a)
    arr.add(b)
    arr.add(c)
    arr.add(a + b + c)
    return arr[0] + arr[1] + arr[2] + arr[3]
}

fun lenCheck(a: Long, b: Long, c: Long): Long {
    val arr: MutableList<Long> = mutableListOf()
    arr.add(a)
    arr.add(b)
    arr.add(c)
    return arr.size.toLong()
}

fun sumSquaresUpTo(n: Long): Long {
    val arr: MutableList<Long> = mutableListOf()
    run {
        var i: Long = 0L
        while (i < n) {
            arr.add(i * i)
            i = i + 1L
        }
    }
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < arr.size.toLong()) {
            total = total + arr[i.toInt()]
            i = i + 1L
        }
    }
    return total
}

fun sumEvensBelow(n: Long): Long {
    val arr: MutableList<Long> = mutableListOf()
    run {
        var i: Long = 0L
        while (i < n) {
            if (i % 2L == 0L) {
                arr.add(i)
            }
            i = i + 1L
        }
    }
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < arr.size.toLong()) {
            total = total + arr[i.toInt()]
            i = i + 1L
        }
    }
    return total
}

fun fillSquares(arr: MutableList<Long>, count: Long): Long {
    run {
        var i: Long = 0L
        while (i < count) {
            arr.add(i * i)
            i = i + 1L
        }
    }
    return count
}

fun squaresThroughAHelper(count: Long): Long {
    val arr: MutableList<Long> = mutableListOf()
    fillSquares(arr, count)
    return arr.size.toLong() * 1000L + arr[(count - 1L).toInt()]
}

fun pushGivesTheArrayBack(): Long {
    var arr: MutableList<Long> = mutableListOf()
    arr = pushed(arr, 10L)
    arr = pushed(arr, 20L)
    return arr[0] + arr[1]
}

fun sumEvensBelowExplicit(n: Long): Long {
    var label: Long = 0L
    var i: Long = 0L
    var arr: MutableList<Long> = mutableListOf()
    machine@ while (true) {
        when (label) {
            0L -> {
                val empty: MutableList<Long> = mutableListOf()
                i = 0L
                arr = empty
                label = 1L
                continue@machine
            }
            1L -> {
                if (i < n) {
                    label = 2L
                    continue@machine
                } else {
                    label = 5L
                    continue@machine
                }
            }
            2L -> {
                if (i % 2L == 0L) {
                    label = 3L
                    continue@machine
                } else {
                    label = 4L
                    continue@machine
                }
            }
            3L -> {
                var next_i: Long = i + 1L
                var next_arr: MutableList<Long> = pushed(arr, i)
                i = next_i
                arr = next_arr
                label = 1L
                continue@machine
            }
            4L -> {
                i = i + 1L
                label = 1L
                continue@machine
            }
            5L -> {
                var i: Long = 0L
                var total: Long = 0L
                while (i < arr.size.toLong()) {
                    i = i + 1L
                    total = total + arr[i.toInt()]
                }
                return total
            }
        }
    }
}
