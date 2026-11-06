data class Point(val x: Double, val y: Double)

fun distanceSquared(a: Point, b: Point): Double {
    val dx: Double = a.x - b.x
    val dy: Double = a.y - b.y
    return dx * dx + dy * dy
}

fun distFromOrigin(bx: Double, by: Double): Double {
    return distanceSquared(Point(0.0, 0.0), Point(bx, by))
}
