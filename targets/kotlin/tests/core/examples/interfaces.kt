import kotlin.system.exitProcess

interface Shape {
    fun area(): Long
    fun scaled(p0: Long): Long
}

interface Named {
    fun id(): Long
}

interface Combiner {
    fun combine(p0: Shape): Long
}

data class Square(val side: Long) : Shape, Named {
    override fun area(): Long {
        return this.side * this.side
    }

    override fun scaled(k: Long): Long {
        return this.side * k
    }

    override fun id(): Long {
        return 1L
    }
}

data class Rect(val w: Long, val h: Long) : Shape {
    override fun area(): Long {
        return this.w * this.h
    }

    override fun scaled(k: Long): Long {
        return this.w * k
    }
}

data class Summer(val base: Long) : Combiner {
    override fun combine(other: Shape): Long {
        return this.base + other.area()
    }
}

data class Holder(val shape: Shape, val tag: Long)

fun totalArea(s: Shape): Long {
    return s.area()
}

fun directCall(): Long {
    return totalArea(Square(3L)) * 100L + totalArea(Rect(4L, 5L))
}

fun pick(which: Long): Shape {
    if (which == 0L) {
        return Square(6L)
    }
    return Rect(2L, 7L)
}

fun chosen(): Long {
    val a: Shape = pick(0L)
    val b: Shape = pick(1L)
    return a.area() * 1000L + b.area() + b.scaled(10L)
}

fun sumAll(): Long {
    val shapes: MutableList<Shape> = mutableListOf()
    shapes.add(Square(2L))
    shapes.add(Rect(3L, 4L))
    shapes.add(Square(5L))
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < shapes.size.toLong()) {
            total = total + shapes[i.toInt()].area()
            i = i + 1L
        }
    }
    return total
}

fun viaField(): Long {
    val h: Holder = Holder(Rect(5L, 6L), 7L)
    return h.shape.area() * 10L + h.tag
}

fun twoInterfaces(): Long {
    val sq: Square = Square(4L)
    val n: Named = sq
    val s: Shape = sq
    return n.id() * 1000L + s.area()
}

fun interfaceArgument(): Long {
    val c: Combiner = Summer(100L)
    return c.combine(Rect(3L, 3L))
}

fun main(): Long {
    return directCall() + chosen() + sumAll() + viaField() + twoInterfaces() + interfaceArgument()
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
