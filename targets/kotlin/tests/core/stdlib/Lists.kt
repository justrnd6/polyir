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

fun <T> first(xs: MutableList<T>): T {
    return xs[0]
}

fun <T> last(xs: MutableList<T>): T {
    return xs[(xs.size.toLong() - 1L).toInt()]
}
