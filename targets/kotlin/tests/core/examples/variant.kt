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

// A variant: the number of its case and the value it holds.
data class Variant(val tag: Long, val value: Any?)

fun safeDivide(a: Long, b: Long): Either<Long, Long> {
    if (b == 0L) {
        return Either.err<Long, Long>(99L)
    }
    return Either.ok<Long, Long>(a / b)
}

fun divideOrDefault(a: Long, b: Long): Long {
    val r: Either<Long, Long> = safeDivide(a, b)
    if (!r.isOk) {
        return r.errValue
    }
    return r.okValue
}

fun eitherCoercion(): Long {
    val r: Either<Double, Long> = Either.ok<Double, Long>(3.0)
    return r.okValue.toLong()
}

fun variantSum(which: Long): Long {
    var v: Variant = Variant(0L, (0L).toLong())
    if (which == 0L) {
        v = Variant(0L, (10L).toLong())
    }
    if (which == 1L) {
        v = Variant(1L, 2.5)
    }
    if (which == 2L) {
        v = Variant(2L, (30L).toLong())
    }
    if (v.tag == 0L) {
        return v.value as Long
    }
    if (v.tag == 1L) {
        return ((v.value as Double).toLong()) * 100L
    }
    return v.value as Long * 1000L
}

fun castRoundTrip(): Long {
    val d: Double = 9.0
    val asI64: Long = d.toLong()
    val backToD: Double = asI64.toDouble()
    return (backToD.toLong()) + asI64
}
