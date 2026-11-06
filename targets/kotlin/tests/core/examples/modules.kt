object Geometry {
    data class Point(val x: Long, val y: Long) {
        fun manhattan(): Long {
            return this.absolute(this.x) + this.absolute(this.y)
        }

        fun absolute(n: Long): Long {
            return if (n < 0L) (0L - n) else n
        }
    }

    fun clamp(n: Long, low: Long, high: Long): Long {
        return if (n < low) low else (if (n > high) high else n)
    }

    fun distance(a: Point, b: Point): Long {
        val step: Point = Point(a.x - b.x, a.y - b.y)
        return clamp(step.manhattan(), 0L, 100L)
    }

    object Shapes {
        interface Shape {
            fun area(): Long
        }

        data class Square(val side: Long) : Shape {
            override fun area(): Long {
                return this.side * this.side
            }
        }

        data class Rect(val w: Long, val h: Long) : Shape {
            override fun area(): Long {
                return this.w * this.h
            }
        }

        fun total(shapes: MutableList<Shape>): Long {
            var sum: Long = 0L
            for (s in shapes) {
                sum = sum + s.area()
            }
            return sum
        }
    }
}

fun main(): Unit {
    val a: Geometry.Point = Geometry.Point(1L, 2L)
    val b: Geometry.Point = Geometry.Point(4L, -2L)
    val shapes: MutableList<Geometry.Shapes.Shape> = mutableListOf(Geometry.Shapes.Square(3L), Geometry.Shapes.Rect(2L, 5L))
    val size: String = if (Geometry.Shapes.total(shapes) > 10L) "big" else "small"
    println("${Geometry.distance(a, b)} ${size} ${larger(3L, 7L, false)} ${larger("x", "y", true)} ${firstOf(mutableListOf("first", "second"))}")
}

fun <T> larger(a: T, b: T, aFirst: Boolean): T {
    return if (aFirst) a else b
}

fun <T> firstOf(xs: MutableList<T>): T {
    return xs[0]
}
