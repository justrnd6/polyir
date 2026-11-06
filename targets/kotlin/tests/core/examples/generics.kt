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

fun boxedI64(): Long {
    val b: Box<Long> = Box(42L)
    return b.getValue()
}

fun boxedBoth(): Long {
    val bi: Box<Long> = Box(100L)
    val bd: Box<Double> = Box(2.0)
    return bi.getValue() + (bd.getValue().toLong())
}

fun pairOfIntAndDouble(): Long {
    val p: Pair_i64_double = Pair_i64_double(7L, 8.5)
    return p.sumAsInt()
}

fun doubledI64(): Long {
    val d: Doubler_i64 = Doubler_i64(21L)
    return d.doubled()
}

fun containerOk(): Long {
    val c: Container<Long> = Container(Either.ok<Long, Long>(42L))
    return c.unwrapOrDefault(0L)
}

fun makePairResult(ok: Long): Either<Pair_i64_i64, Long> {
    if (ok == 0L) {
        return Either.err<Pair_i64_i64, Long>(-1L)
    }
    return Either.ok<Pair_i64_i64, Long>(Pair_i64_i64(11L, 22L))
}

fun pairResultOrErr(ok: Long): Long {
    val r: Either<Pair_i64_i64, Long> = makePairResult(ok)
    if (!r.isOk) {
        return r.errValue
    }
    val p: Pair_i64_i64 = r.okValue
    return p.a * 100L + p.b
}

data class Box<T>(val value: T) {
    fun getValue(): T {
        return this.value
    }
}

data class Pair_i64_double(val a: Long, val b: Double) {
    fun sumAsInt(): Long {
        return (this.a.toLong()) + (this.b.toLong())
    }
}

data class Doubler_i64(val value: Long) {
    fun doubled(): Long {
        return this.value + this.value
    }
}

data class Container<T>(val value: Either<T, Long>) {
    fun unwrapOrDefault(def: T): T {
        if (!this.value.isOk) {
            return def
        }
        return this.value.okValue
    }
}

data class Pair_i64_i64(val a: Long, val b: Long) {
    fun sumAsInt(): Long {
        return (this.a.toLong()) + (this.b.toLong())
    }
}
