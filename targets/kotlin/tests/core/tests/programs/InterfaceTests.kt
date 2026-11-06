// An `either`: an ok value or an error, and which one it holds.
class Either<T, E> private constructor(val isOk: Boolean, private val held: Any?) {
    @Suppress("UNCHECKED_CAST")
    val okValue: T get() = held as T

    @Suppress("UNCHECKED_CAST")
    val errValue: E get() = held as E

    override fun equals(other: Any?): Boolean = other is Either<*, *> && isOk == other.isOk && held == other.held

    override fun hashCode(): Int = 31 * isOk.hashCode() + (held?.hashCode() ?: 0)

    companion object {
        fun <T, E> ok(value: T): Either<T, E> = Either(true, value)

        fun <T, E> err(value: E): Either<T, E> = Either(false, value)
    }
}

fun checkConversionAtACall(): Double {
    return offInt(directCall(), 920L)
}

fun checkConversionAtAReturn(): Double {
    return offInt(chosen(), 36034L)
}

fun checkDarrayOfInterfaceValues(): Double {
    return offInt(sumAll(), 41L)
}

fun checkInterfaceAsAStructField(): Double {
    return offInt(viaField(), 307L)
}

fun checkOneStructTwoInterfaces(): Double {
    return offInt(twoInterfaces(), 1016L)
}

fun checkInterfaceMethodTakingAnInterface(): Double {
    return offInt(interfaceArgument(), 109L)
}

fun checkDispatchFollowsTheValue(): Double {
    val a: Shape = Square(5L)
    val b: Shape = Rect(5L, 2L)
    return offInt(a.area(), 25L) + offInt(b.area(), 10L)
}

fun checkSameMethodNameOnTwoStructs(): Double {
    val s: Square = Square(3L)
    val r: Rect = Rect(3L, 4L)
    return offInt(s.area() + r.area(), 21L)
}

fun checkConvertedValueIsACopy(): Double {
    val s: Square = Square(4L)
    val a: Shape = s
    val b: Shape = s
    return offInt(a.area() + b.area(), 32L)
}

interface Scaler {
    fun half(p0: Float): Float
    fun offset(p0: Int): Long
}

data class Halver(val base: Long) : Scaler {
    override fun half(x: Float): Float {
        return x / 2.0f
    }

    override fun offset(by: Int): Long {
        return this.base + (by.toLong())
    }
}

fun checkArgumentWidthsFollowTheInterface(): Double {
    val s: Scaler = Halver(40L)
    return off(s.half(9.0f), 4.5f) + offInt(s.offset(2), 42L)
}

fun checkInterfaceInAnEither(): Double {
    val r: Either<Shape, Long> = Either.ok<Shape, Long>(Rect(4L, 4L))
    if (!r.isOk) {
        return 1.0
    }
    val s: Shape = r.okValue
    return offInt(s.area(), 16L)
}

fun checkInterfaceCapturedByAClosure(): Double {
    val s: Shape = Square(7L)
    val f: () -> Long = { s.area() }
    return offInt(f(), 49L)
}

fun checkInterfaceThroughAVar(): Double {
    var s: Shape = Square(2L)
    val first: Long = s.area()
    s = Rect(3L, 5L)
    return offInt(first * 100L + s.area(), 415L)
}

interface Weighted {
    fun weight(): Long
}

fun totalWeight(a: Weighted, b: Weighted): Long {
    return a.weight() + b.weight()
}

fun checkGenericStructConvertsToAnInterface(): Double {
    val a: Cell<Long> = Cell(5L)
    val b: Cell<Double> = Cell(2.5)
    return offInt(totalWeight(a, b), 14L) + offInt(a.get(), 5L) + off(b.get().toFloat(), 2.5f)
}

fun checkGenericInterfaceValuesInADarray(): Double {
    val all: MutableList<Weighted> = mutableListOf()
    val a: Cell<Long> = Cell(1L)
    all.add(a)
    all.add(a)
    return offInt(all[0].weight() + all[1].weight(), 14L)
}

data class Cell<T>(val value: T) : Weighted {
    override fun weight(): Long {
        return 7L
    }

    fun get(): T {
        return this.value
    }
}
