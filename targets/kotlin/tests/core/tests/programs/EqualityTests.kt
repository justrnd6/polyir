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

data class Point(val x: Long, val y: Long)
data class Line(val a: Point, val b: Point)
data class Named(val name: String, val tags: MutableList<Long>)
data class Counter(var n: Long, val id: Long)

data class Digit(val v: Long) {
    fun equals(other: Digit): Boolean {
        return this.v % 10L == other.v % 10L
    }
}

data class Wrapped(val inner: Either<Long, String>)

fun checkStructsAreEqualFieldByField(): Double {
    val p: Point = Point(1L, 2L)
    val q: Point = Point(1L, 2L)
    val r: Point = Point(1L, 3L)
    var score: Long = 0L
    if (p == q) {
        score = score + 1L
    }
    if (p != r) {
        score = score + 2L
    }
    if (p == r) {
        score = score + 100L
    }
    return offInt(score, 3L)
}

fun checkNestedStructsAreEqualRecursively(): Double {
    val p: Point = Point(1L, 2L)
    val z: Point = Point(3L, 4L)
    val a: Line = Line(p, z)
    val b: Line = Line(Point(1L, 2L), Point(3L, 4L))
    val c: Line = Line(z, p)
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a != c) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkAStringAndAListInsideAStructAreComparedByContent(): Double {
    val t1: MutableList<Long> = mutableListOf()
    t1.add(1L)
    val t2: MutableList<Long> = mutableListOf()
    t2.add(1L)
    val a: Named = Named("a" + "b", t1)
    val b: Named = Named("ab", t2)
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    t2.add(2L)
    if (a != b) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkStringsCompareByCharacters(): Double {
    val x: String = "match"
    val y: String = "ma" + "tch"
    var score: Long = 0L
    if (x == y) {
        score = score + 1L
    }
    if (x != "other") {
        score = score + 2L
    }
    if (x === y) {
        score = score + 100L
    }
    return offInt(score, 3L)
}

fun checkAStructWithVarFieldsIsComparedByWhatItHoldsNow(): Double {
    val a: Counter = Counter(1L, 2L)
    val b: Counter = Counter(1L, 2L)
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    b.n = 5L
    if (a != b) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkIdentityIsTheSameObject(): Double {
    val a: Counter = Counter(1L, 2L)
    val b: Counter = Counter(1L, 2L)
    val alias: Counter = a
    var score: Long = 0L
    if (a === alias) {
        score = score + 1L
    }
    if (a !== b) {
        score = score + 2L
    }
    if (a === b) {
        score = score + 100L
    }
    return offInt(score, 3L)
}

fun checkAnImmutableStructComparedByIdentityIsAnObject(): Double {
    val p: Point = Point(1L, 2L)
    val q: Point = Point(1L, 2L)
    val same: Point = p
    var score: Long = 0L
    if (p === same) {
        score = score + 1L
    }
    if (p !== q) {
        score = score + 2L
    }
    if (p == q) {
        score = score + 4L
    }
    return offInt(score, 7L)
}

fun checkAValStructIsADifferentObjectThatIsEqual(): Double {
    val a: Counter = Counter(1L, 2L)
    val b: Counter = a.copy()
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a !== b) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkAnEqualsMethodDecidesWhatEqualMeans(): Double {
    val a: Digit = Digit(13L)
    val b: Digit = Digit(23L)
    val c: Digit = Digit(24L)
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a != c) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkListsAreEqualElementByElement(): Double {
    val a: MutableList<Long> = mutableListOf()
    val b: MutableList<Long> = mutableListOf()
    a.add(1L)
    a.add(2L)
    b.add(1L)
    b.add(2L)
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    b.add(3L)
    if (a != b) {
        score = score + 2L
    }
    if (a === b) {
        score = score + 100L
    }
    return offInt(score, 3L)
}

fun checkListsOfStructsAreEqualDeeply(): Double {
    val a: MutableList<Point> = mutableListOf()
    val b: MutableList<Point> = mutableListOf()
    a.add(Point(1L, 2L))
    b.add(Point(1L, 2L))
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    a.add(Point(5L, 6L))
    b.add(Point(5L, 7L))
    if (a != b) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload(): Double {
    val a: Either<Long, Long> = Either.ok<Long, Long>(5L)
    val b: Either<Long, Long> = Either.ok<Long, Long>(5L)
    val c: Either<Long, Long> = Either.err<Long, Long>(5L)
    val d: Either<Long, Long> = Either.ok<Long, Long>(6L)
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a != c) {
        score = score + 2L
    }
    if (a != d) {
        score = score + 4L
    }
    return offInt(score, 7L)
}

fun checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload(): Double {
    val a: Variant = Variant(1L, 2.5)
    val b: Variant = Variant(1L, 2.5)
    val c: Variant = Variant(0L, (2L).toLong())
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a != c) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkAStructHoldingAnEitherIsComparedThroughIt(): Double {
    val a: Wrapped = Wrapped(Either.ok<Long, String>(1L))
    val b: Wrapped = Wrapped(Either.ok<Long, String>(1L))
    val c: Wrapped = Wrapped(Either.err<Long, String>("no"))
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a != c) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkNanIsNotEqualToItself(): Double {
    val nan: Double = 0.0 / 0.0
    var score: Long = 0L
    if (nan == nan) {
        score = score + 100L
    }
    if (nan != nan) {
        score = score + 1L
    }
    return offInt(score, 1L)
}
