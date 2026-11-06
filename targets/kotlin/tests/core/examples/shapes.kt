data class Rect(val width: Long, val height: Long)

fun area(r: Rect): Long {
    return r.width * r.height
}
