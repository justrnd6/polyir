fun offText(got: String, want: String): Double {
    if (got == want) {
        return 0.0
    }
    return 1.0
}

fun checkFirstAndLast(): Double {
    val xs: MutableList<Long> = mutableListOf(3L, 1L, 4L)
    val words: MutableList<String> = mutableListOf("a", "b")
    return offText("${first(xs)} ${last(xs)} ${first(words)}${last(words)}", "3 4 ab")
}

fun checkDrop(): Double {
    val xs: MutableList<Long> = mutableListOf(1L, 2L, 3L)
    val rest: MutableList<Long> = drop(xs, 1L)
    val none: MutableList<Long> = drop(xs, 5L)
    return offText("${rest.size.toLong()} ${rest[0]} ${rest[1]} ${none.size.toLong()}", "2 2 3 0")
}

fun <T> first(xs: MutableList<T>): T {
    return xs[0]
}

fun <T> last(xs: MutableList<T>): T {
    return xs[(xs.size.toLong() - 1L).toInt()]
}

fun <T> drop(xs: MutableList<T>, n: Long): MutableList<T> {
    val out: MutableList<T> = mutableListOf()
    run {
        var i: Long = n
        while (i < xs.size.toLong()) {
            out.add(xs[i.toInt()])
            i = i + 1L
        }
    }
    return out
}
