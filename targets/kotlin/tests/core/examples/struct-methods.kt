data class Point(val x: Double, val y: Double) {
    fun dist(): Double {
        return this.x * this.x + this.y * this.y
    }
}

data class Circle(val r: Double) {
    fun dist(): Double {
        return this.r * this.r
    }
}

fun distSum(px: Double, py: Double, r: Double): Double {
    val p: Point = Point(px, py)
    val c: Circle = Circle(r)
    return p.dist() + c.dist()
}

data class Pair(val a: Long, val b: Long) {
    fun sum(): Long {
        return this.a + this.b
    }

    fun doubledSum(): Long {
        return this.sum() * 2L
    }
}

fun doubledSumCheck(a: Long, b: Long): Long {
    val p: Pair = Pair(a, b)
    return p.doubledSum()
}
