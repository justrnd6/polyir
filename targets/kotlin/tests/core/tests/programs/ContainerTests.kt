// A push used as a value gives the list, as in polyir (Kotlin's add gives whether it changed the list).
private fun <T> pushed(items: MutableList<T>, item: T): MutableList<T> {
    items.add(item)
    return items
}

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

// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero. `set(value, row, col)` writes one.
class Sparse<T>(private val zero: T) {
    private val rows = mutableListOf<Long>()
    private val cols = mutableListOf<Long>()
    private val values = mutableListOf<T>()

    val size: Int get() = values.size

    fun get(row: Long, col: Long): T {
        var found = zero
        for (i in values.indices) {
            if (rows[i] == row && cols[i] == col) {
                found = values[i]
            }
        }
        return found
    }

    fun set(value: T, row: Long, col: Long) {
        rows.add(row)
        cols.add(col)
        values.add(value)
    }

    fun copy(): Sparse<T> {
        val out = Sparse(zero)
        out.rows.addAll(rows)
        out.cols.addAll(cols)
        out.values.addAll(values)
        return out
    }
}

// A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(value, i, j)`
// writes one.
class Grid<T>(val dims: List<Int>, zero: T) {
    private val cells = MutableList(dims.fold(1) { count, size -> count * size }) { zero }

    private fun place(indices: LongArray): Int = dims.indices.fold(0) { at, i -> at * dims[i] + indices[i].toInt() }

    fun get(vararg indices: Long): T = cells[place(indices)]

    fun set(value: T, vararg indices: Long) {
        cells[place(indices)] = value
    }

    fun copy(): Grid<T> {
        val out = Grid(dims, cells[0])
        for (i in cells.indices) {
            out.cells[i] = cells[i]
        }
        return out
    }
}

// What `throw` throws: the code thrown.
class Thrown(val value: Long) : RuntimeException("thrown $value")

// Stops the program with a message, as polyir's `fail` does.
private fun fail(message: String): Nothing {
    System.err.println("trap: " + message)
    exitProcess(1)
}

data class Bag(val items: MutableList<Long>, val total: Long)
data class Pt(val x: Long, val y: Long)

fun pushThree(xs: MutableList<Long>): Long {
    xs.add(3L)
    return xs.size.toLong()
}

fun pushMany(xs: MutableList<Long>, count: Long): Long {
    run {
        var i: Long = 0L
        while (i < count) {
            xs.add(i * 2L)
            i = i + 1L
        }
    }
    return xs.size.toLong()
}

fun checkAPushInsideAFunctionIsSeenByTheCaller(): Double {
    val xs: MutableList<Long> = mutableListOf()
    xs.add(1L)
    pushThree(xs)
    return offInt(xs.size.toLong(), 2L) + offInt(xs[1], 3L)
}

fun checkManyPushesInsideAFunctionLeaveTheCallerValid(): Double {
    val xs: MutableList<Long> = mutableListOf()
    xs.add(7L)
    pushMany(xs, 100L)
    return offInt(xs.size.toLong(), 101L) + offInt(xs[0], 7L) + offInt(xs[1], 0L) + offInt(xs[100], 198L)
}

fun checkTwoNamesForOneDarrayShareIt(): Double {
    val a: MutableList<Long> = mutableListOf()
    val b: MutableList<Long> = a
    a.add(5L)
    b.add(6L)
    return offInt(a.size.toLong(), 2L) + offInt(b.size.toLong(), 2L) + offInt(a[1], 6L) + offInt(b[0], 5L)
}

fun checkAnElementWrittenInAFunctionIsSeenByTheCaller(): Double {
    val xs: MutableList<Long> = mutableListOf()
    xs.add(1L)
    xs.add(2L)
    overwriteFirst(xs, 40L)
    return offInt(xs[0], 40L) + offInt(xs[1], 2L)
}

fun overwriteFirst(xs: MutableList<Long>, value: Long): Long {
    xs[0] = value
    return 0L
}

fun makeFilled(count: Long): MutableList<Long> {
    val xs: MutableList<Long> = mutableListOf()
    run {
        var i: Long = 0L
        while (i < count) {
            xs.add(i + 1L)
            i = i + 1L
        }
    }
    return xs
}

fun checkAReturnedDarrayIsTheOneTheFunctionBuilt(): Double {
    val xs: MutableList<Long> = makeFilled(4L)
    xs.add(50L)
    return offInt(xs.size.toLong(), 5L) + offInt(xs[3], 4L) + offInt(xs[4], 50L)
}

fun checkAStructFieldIsTheSameDarray(): Double {
    val xs: MutableList<Long> = mutableListOf()
    val bag: Bag = Bag(xs, 0L)
    xs.add(8L)
    bag.items.add(9L)
    return offInt(bag.items.size.toLong(), 2L) + offInt(xs.size.toLong(), 2L) + offInt(bag.items[1], 9L)
}

fun checkAMethodStylePushOnAFieldWorks(): Double {
    val bag: Bag = Bag(mutableListOf(), 0L)
    bag.items.add(4L)
    bag.items.add(5L)
    return offInt(bag.items.size.toLong(), 2L) + offInt(bag.items[1], 5L)
}

fun checkConcatenationMakesANewDarray(): Double {
    val a: MutableList<Long> = mutableListOf()
    val b: MutableList<Long> = mutableListOf()
    a.add(1L)
    b.add(2L)
    val c: MutableList<Long> = (a + b).toMutableList()
    a.add(99L)
    return offInt(c.size.toLong(), 2L) + offInt(c[0], 1L) + offInt(c[1], 2L) + offInt(a.size.toLong(), 2L)
}

fun checkAClosureSharesTheDarrayItCaptures(): Double {
    val xs: MutableList<Long> = mutableListOf()
    val add: (Long) -> Long = fun(v: Long): Long {
        xs.add(v)
        return xs.size.toLong()
    }
    add(10L)
    add(20L)
    return offInt(xs.size.toLong(), 2L) + offInt(xs[1], 20L)
}

fun checkPushGivesBackTheSameDarray(): Double {
    var xs: MutableList<Long> = mutableListOf()
    xs = pushed(xs, 5L)
    xs = pushed(xs, 6L)
    return offInt(xs.size.toLong(), 2L) + offInt(xs[1], 6L)
}

fun checkReassigningAVarSwitchesDarrays(): Double {
    var xs: MutableList<Long> = mutableListOf()
    val other: MutableList<Long> = mutableListOf()
    other.add(1L)
    other.add(2L)
    other.add(3L)
    xs = other
    xs.add(4L)
    return offInt(other.size.toLong(), 4L) + offInt(xs.size.toLong(), 4L)
}

fun checkAListOfListsSharesItsInnerLists(): Double {
    val outer: MutableList<MutableList<Long>> = mutableListOf()
    val a: MutableList<Long> = mutableListOf()
    a.add(1L)
    a.add(2L)
    outer.add(a)
    val b: MutableList<Long> = mutableListOf()
    b.add(10L)
    outer.add(b)
    a.add(3L)
    val first: MutableList<Long> = outer[0]
    val second: MutableList<Long> = outer[1]
    return offInt(outer.size.toLong(), 2L) + offInt(first.size.toLong(), 3L) + offInt(first[2], 3L) + offInt(second[0], 10L)
}

fun checkADarrayOfStructsKeepsTheStructs(): Double {
    val ps: MutableList<Pt> = mutableListOf()
    ps.add(Pt(1L, 2L))
    ps.add(Pt(3L, 4L))
    val p: Pt = ps[1]
    return offInt(ps.size.toLong(), 2L) + offInt(p.x, 3L) + offInt(p.y, 4L)
}

fun checkAThousandPushesKeepEveryElement(): Double {
    val xs: MutableList<Long> = mutableListOf()
    run {
        var i: Long = 0L
        while (i < 1000L) {
            xs.add(i)
            i = i + 1L
        }
    }
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < xs.size.toLong()) {
            total = total + xs[i.toInt()]
            i = i + 1L
        }
    }
    return offInt(xs.size.toLong(), 1000L) + offInt(total, 499500L)
}

fun checkAListInsideAnEitherIsTheSameList(): Double {
    val xs: MutableList<Long> = mutableListOf()
    xs.add(4L)
    val r: Either<MutableList<Long>, Long> = Either.ok<MutableList<Long>, Long>(xs)
    xs.add(5L)
    val back: MutableList<Long> = r.okValue
    return offInt(back.size.toLong(), 2L) + offInt(back[1], 5L)
}

fun fillSparse(sp: Sparse<Long>): Long {
    sp.set(5L, 1L, 1L)
    sp.set(7L, 2L, 3L)
    return 0L
}

fun checkASparseWrittenInAFunctionIsSeenByTheCaller(): Double {
    val sp: Sparse<Long> = Sparse(0L)
    fillSparse(sp)
    return offInt(sp.get(1L, 1L), 5L) + offInt(sp.get(2L, 3L), 7L) + offInt(sp.get(9L, 9L), 0L) + offInt(sp.size.toLong(), 2L)
}

fun checkLastWriteWinsForASparse(): Double {
    val sp: Sparse<Long> = Sparse(0L)
    sp.set(1L, 3L, 3L)
    sp.set(2L, 3L, 3L)
    sp.set(3L, 3L, 3L)
    return offInt(sp.get(3L, 3L), 3L)
}

fun checkOneDimensionalSparseSharesToo(): Double {
    val sp: Sparse<Long> = Sparse(0L)
    val alias: Sparse<Long> = sp
    alias.set(100L, 0L, 0L)
    sp.set(200L, 7L, 0L)
    return offInt(sp.get(0L, 0L), 100L) + offInt(alias.get(7L, 0L), 200L)
}

fun fillMap(hm: MutableMap<Long, Long>): Long {
    hm[1L] = 10L
    hm[2L] = 20L
    hm[1L] = 11L
    return 0L
}

fun checkAHashmapWrittenInAFunctionIsSeenByTheCaller(): Double {
    val hm: MutableMap<Long, Long> = mutableMapOf()
    fillMap(hm)
    return offInt(hm[1L] ?: 0L, 11L) + offInt(hm[2L] ?: 0L, 20L) + offInt(hm[3L] ?: 0L, 0L) + offInt(hm.size.toLong(), 2L)
}

fun checkTwoNamesForOneHashmapShareIt(): Double {
    val a: MutableMap<Long, Long> = mutableMapOf()
    val b: MutableMap<Long, Long> = a
    a[5L] = 50L
    b[6L] = 60L
    return offInt(a.size.toLong(), 2L) + offInt(b[5L] ?: 0L, 50L) + offInt(a[6L] ?: 0L, 60L)
}

fun checkAStringKeyedHashmapIsShared(): Double {
    val hm: MutableMap<String, Long> = mutableMapOf()
    addWord(hm, "one", 1L)
    addWord(hm, "two", 2L)
    return offInt(hm["one"] ?: 0L, 1L) + offInt(hm["two"] ?: 0L, 2L) + offInt(hm.size.toLong(), 2L)
}

fun addWord(hm: MutableMap<String, Long>, word: String, value: Long): Long {
    hm[word] = value
    return 0L
}

fun checkAHashmapOfListsKeepsTheLists(): Double {
    val hm: MutableMap<Long, MutableList<Long>> = mutableMapOf()
    val l: MutableList<Long> = mutableListOf()
    hm[7L] = l
    l.add(5L)
    l.add(6L)
    val got: MutableList<Long> = hm[7L] ?: mutableListOf()
    return offInt(got.size.toLong(), 2L) + offInt(got[1], 6L)
}

fun fillGrid(g: Grid<Long>): Long {
    run {
        var r: Long = 0L
        while (r < 2L) {
            run {
                var c: Long = 0L
                while (c < 3L) {
                    g.set(r * 10L + c, r, c)
                    c = c + 1L
                }
            }
            r = r + 1L
        }
    }
    return 0L
}

fun checkAGridFilledInAFunctionIsSeenByTheCaller(): Double {
    val g: Grid<Long> = Grid(listOf(2, 3), 0L)
    fillGrid(g)
    return offInt(g.get(1L, 2L), 12L) + offInt(g.get(0L, 1L), 1L)
}

fun checkTwoNamesForOneGridShareIt(): Double {
    val a: Grid<Long> = Grid(listOf(2, 3), 0L)
    val b: Grid<Long> = a
    a.set(9L, 1L, 1L)
    return offInt(b.get(1L, 1L), 9L)
}

fun checkAGridStartsZeroed(): Double {
    val t: Grid<Long> = Grid(listOf(2, 2, 2), 0L)
    t.set(5L, 1L, 1L, 1L)
    return offInt(t.get(0L, 1L, 0L), 0L) + offInt(t.get(1L, 1L, 1L), 5L)
}

fun checkDarrayCopyIsIndependent(): Double {
    val a: MutableList<Long> = mutableListOf()
    a.add(1L)
    a.add(2L)
    val b: MutableList<Long> = a.toMutableList()
    b.add(3L)
    a.add(9L)
    return offInt(a.size.toLong(), 3L) + offInt(b.size.toLong(), 3L) + offInt(b[2], 3L) + offInt(a[2], 9L) + offInt(b[0], 1L)
}

fun checkGridAndTensorCopyAreIndependent(): Double {
    val g: Grid<Long> = Grid(listOf(2, 2), 0L)
    g.set(5L, 1L, 1L)
    val h: Grid<Long> = g.copy()
    g.set(6L, 1L, 1L)
    val t: Grid<Long> = Grid(listOf(2, 2, 2), 0L)
    t.set(7L, 1L, 0L, 1L)
    val u: Grid<Long> = t.copy()
    t.set(8L, 1L, 0L, 1L)
    return offInt(h.get(1L, 1L), 5L) + offInt(g.get(1L, 1L), 6L) + offInt(u.get(1L, 0L, 1L), 7L)
}

fun checkSparseCopyIsIndependent(): Double {
    val s: Sparse<Long> = Sparse(0L)
    s.set(7L, 1L, 2L)
    val c: Sparse<Long> = s.copy()
    s.set(8L, 3L, 4L)
    c.set(9L, 5L, 6L)
    return offInt(s.size.toLong(), 2L) + offInt(c.size.toLong(), 2L) + offInt(c.get(1L, 2L), 7L)
}

fun checkHashmapCopyIsIndependent(): Double {
    val m: MutableMap<Long, Long> = mutableMapOf()
    m[1L] = 10L
    val c: MutableMap<Long, Long> = m.toMutableMap()
    m[2L] = 20L
    c[3L] = 30L
    return offInt(m.size.toLong(), 2L) + offInt(c.size.toLong(), 2L) + offInt(c[1L] ?: 0L, 10L) + offInt(c[3L] ?: 0L, 30L)
}

fun checkCopyOfAStringKeyedHashmap(): Double {
    val m: MutableMap<String, Long> = mutableMapOf()
    m["a"] = 1L
    val c: MutableMap<String, Long> = m.toMutableMap()
    m["b"] = 2L
    return offInt(c["a"] ?: 0L, 1L) + offInt(c.size.toLong(), 1L)
}

fun makeBag(n: Long): MutableList<Long> {
    val d: MutableList<Long> = mutableListOf()
    d.add(n)
    d.add(n + 1L)
    return d
}

fun sumGrid(g: Grid<Long>): Long {
    return g.get(0L, 0L) + g.get(1L, 1L)
}

fun holdsOnto(store: MutableList<MutableList<Long>>, d: MutableList<Long>): Long {
    store.add(d)
    return 0L
}

fun checkAReturnedContainerSurvives(): Double {
    val a: MutableList<Long> = makeBag(10L)
    val b: MutableList<Long> = makeBag(20L)
    return offInt(a[0] + a[1] + b[0] + b[1], 62L)
}

fun checkAContainerLentToACalleeIsStillUsableAfterwards(): Double {
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 50L) {
            val g: Grid<Long> = Grid(listOf(2, 2), 0L)
            g.set(i, 0L, 0L)
            g.set(1L, 1L, 1L)
            total = total + sumGrid(g) + g.get(0L, 0L)
            i = i + 1L
        }
    }
    return offInt(total, 2L * 1225L + 50L + 0L)
}

fun checkAContainerGivenToACalleeThatKeepsItSurvives(): Double {
    val store: MutableList<MutableList<Long>> = mutableListOf()
    run {
        var i: Long = 0L
        while (i < 3L) {
            val d: MutableList<Long> = mutableListOf()
            d.add(i * 10L)
            holdsOnto(store, d)
            i = i + 1L
        }
    }
    return offInt(store[0][0] + store[1][0] + store[2][0], 30L)
}

fun checkAContainerReturnedFromInsideAnIfSurvives(): Double {
    val keep: MutableList<Long> = mutableListOf()
    keep.add(4L)
    val other: MutableList<Long> = mutableListOf()
    other.add(5L)
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 20L) {
            val scratch: MutableList<Long> = mutableListOf()
            scratch.add(i)
            if (i == 7L) {
                total = total + scratch[0]
            }
            i = i + 1L
        }
    }
    return offInt(total + keep[0] + other[0], 16L)
}

fun earlyExit(n: Long): Long {
    val scratch: MutableList<Long> = mutableListOf()
    scratch.add(n)
    if (n > 5L) {
        return scratch[0] * 2L
    }
    scratch.add(1L)
    return scratch.size.toLong() + scratch[0]
}

fun checkAReturnInsideAnIfStillReleasesAndReturnsTheRightValue(): Double {
    return offInt(earlyExit(9L) * 100L + earlyExit(2L), 18L * 100L + 4L)
}

fun greeting(name: String): String {
    val hello: String = "hello, "
    val full: String = hello + name + "!"
    return full
}

fun shout(s: String): Long {
    if (s == "hi!") {
        return 1L
    }
    return 0L
}

fun counterFrom(start: Long): () -> Long {
    var n: Long = start
    val next: () -> Long = fun(): Long {
        n = n + 1L
        return n
    }
    return next
}

fun applyTwice(f: () -> Long): Long {
    f()
    return f()
}

fun checkAReturnedStringSurvives(): Double {
    val a: String = greeting("ann")
    val b: String = greeting("bo")
    if (a != "hello, ann!") {
        return 1.0
    }
    if (b != "hello, bo!") {
        return 1.0
    }
    return 0.0
}

fun checkAStringLentToACalleeIsUsableAfterwards(): Double {
    var hits: Long = 0L
    run {
        var i: Long = 0L
        while (i < 20L) {
            val s: String = "h" + "i" + "!"
            hits = hits + shout(s)
            if (s == "hi!") {
                hits = hits + 1L
            }
            i = i + 1L
        }
    }
    return offInt(hits, 40L)
}

fun checkAStringUsedAsAHashmapKeyIsKept(): Double {
    val m: MutableMap<String, Long> = mutableMapOf()
    run {
        var i: Long = 0L
        while (i < 3L) {
            val key: String = "k" + "ey"
            m[key] = i
            i = i + 1L
        }
    }
    return offInt(m["key"] ?: 0L, 2L) + offInt(m.size.toLong(), 1L)
}

fun checkAClosureThatIsReturnedKeepsItsBox(): Double {
    val c: () -> Long = counterFrom(10L)
    c()
    return offInt(c(), 12L)
}

fun checkAClosureGivenToACalleeKeepsWorking(): Double {
    var n: Long = 0L
    val tick: () -> Long = fun(): Long {
        n = n + 5L
        return n
    }
    return offInt(applyTwice(tick), 10L) + offInt(n, 10L)
}

fun checkAClosureOnlyCalledLocallyInALoop(): Double {
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 50L) {
            var count: Long = i
            val bump: () -> Long = fun(): Long {
                count = count + 1L
                return count
            }
            bump()
            total = total + bump() - i
            i = i + 1L
        }
    }
    return offInt(total, 100L)
}

fun checkTwoClosuresShareOneCapturedName(): Double {
    var n: Long = 0L
    val inc: () -> Long = fun(): Long {
        n = n + 1L
        return n
    }
    val read: () -> Long = { n * 10L }
    inc()
    inc()
    return offInt(read(), 20L)
}

data class Pocket(var items: MutableList<Long>)

fun madeHere(): MutableList<Long> {
    val d: MutableList<Long> = mutableListOf()
    d.add(1L)
    return d
}

fun madeThroughAnother(): MutableList<Long> {
    val d: MutableList<Long> = madeHere()
    d.add(2L)
    return d
}

fun madeInPlace(): MutableList<Long> {
    return mutableListOf()
}

fun sameAsArgument(d: MutableList<Long>): MutableList<Long> {
    return d
}

fun elementOf(xs: MutableList<MutableList<Long>>): MutableList<Long> {
    return xs[0]
}

fun fieldOf(p: Pocket): MutableList<Long> {
    return p.items
}

fun keptAndReturned(store: MutableList<MutableList<Long>>): MutableList<Long> {
    val d: MutableList<Long> = mutableListOf()
    store.add(d)
    return d
}

fun sometimesTheArgument(flag: Long, d: MutableList<Long>): MutableList<Long> {
    if (flag == 0L) {
        return d
    }
    return mutableListOf()
}

fun checkAFreshResultIsUsableAndReleased(): Double {
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 100L) {
            val a: MutableList<Long> = madeHere()
            val b: MutableList<Long> = madeThroughAnother()
            val c: MutableList<Long> = madeInPlace()
            c.add(i)
            total = total + a.size.toLong() + b.size.toLong() + c.size.toLong()
            i = i + 1L
        }
    }
    return offInt(total, 100L * 4L)
}

fun checkAnAliasReturnedByACallIsNotReleasedUnderItsOwner(): Double {
    val src: MutableList<Long> = mutableListOf()
    src.add(7L)
    val same: MutableList<Long> = sameAsArgument(src)
    same.add(8L)
    val outer: MutableList<MutableList<Long>> = mutableListOf()
    outer.add(src)
    val inner: MutableList<Long> = elementOf(outer)
    val pocket: Pocket = Pocket(src)
    val field: MutableList<Long> = fieldOf(pocket)
    val maybe: MutableList<Long> = sometimesTheArgument(0L, src)
    return offInt(src.size.toLong(), 2L) + offInt(inner.size.toLong(), 2L) + offInt(field.size.toLong(), 2L) + offInt(maybe.size.toLong(), 2L)
}

fun checkAnObjectKeptByTheCalleeSurvivesItsCaller(): Double {
    val store: MutableList<MutableList<Long>> = mutableListOf()
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 5L) {
            val d: MutableList<Long> = keptAndReturned(store)
            d.add(i)
            i = i + 1L
        }
    }
    run {
        var j: Long = 0L
        while (j < 5L) {
            total = total + store[j.toInt()][0]
            j = j + 1L
        }
    }
    return offInt(total, 10L)
}

fun handedOn(): MutableList<Long> {
    val a: MutableList<Long> = madeHere()
    return a
}

fun checkAResultHandedOnAgainIsStillFresh(): Double {
    val a: MutableList<Long> = handedOn()
    return offInt(a[0], 1L)
}

fun checkADiscardedFreshResultIsHarmless(): Double {
    madeHere()
    madeThroughAnother()
    return 0.0
}

fun madeStruct(n: Long): Pocket {
    val p: Pocket = Pocket(mutableListOf())
    p.items.add(n)
    return p
}

fun makeLabel(a: String): String {
    val s: String = a + "-" + a
    return s
}

fun checkAStructAndAStringHandedOverByACall(): Double {
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 50L) {
            val p: Pocket = madeStruct(i)
            val s: String = makeLabel("x")
            if (s == "x-x") {
                total = total + p.items[0] + 1L
            }
            i = i + 1L
        }
    }
    return offInt(total, 1225L + 50L)
}

data class Shelf(var books: MutableList<Long>, var tags: String)
data class Cabinet(var drawer: MutableList<Long>)
data class Crate(var items: MutableList<Long>)

fun stockedShelf(n: Long): Shelf {
    val s: Shelf = Shelf(mutableListOf(), "a" + "b")
    s.books.add(n)
    s.books.add(n + 1L)
    return s
}

fun openDrawer(c: Cabinet): MutableList<Long> {
    return c.drawer
}

fun checkAnOwnedFieldIsUsableThenReleasedWithItsStruct(): Double {
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 100L) {
            val s: Shelf = stockedShelf(i)
            total = total + s.books.size.toLong() + s.books[0] - i
            if (s.tags == "ab") {
                total = total + 1L
            }
            i = i + 1L
        }
    }
    return offInt(total, 100L * 3L)
}

fun checkAReplacedFieldValueIsReleasedAndTheNewOneWorks(): Double {
    val s: Shelf = stockedShelf(5L)
    s.books = mutableListOf()
    s.books.add(40L)
    s.tags = "x" + "y"
    if (s.tags != "xy") {
        return 1.0
    }
    return offInt(s.books.size.toLong(), 1L) + offInt(s.books[0], 40L)
}

fun checkAFieldThatIsReadOutIsNeverReleasedWithItsStruct(): Double {
    val c: Cabinet = Cabinet(mutableListOf())
    c.drawer.add(9L)
    val drawer: MutableList<Long> = openDrawer(c)
    drawer.add(10L)
    return offInt(c.drawer.size.toLong(), 2L) + offInt(drawer[1], 10L)
}

fun checkACopiedStructSharesNothingItMustNotFree(): Double {
    val a: Crate = Crate(mutableListOf())
    a.items.add(3L)
    val b: Crate = a.copy()
    b.items.add(4L)
    return offInt(a.items.size.toLong(), 2L) + offInt(b.items.size.toLong(), 2L)
}

fun checkAListOfListsReleasesItsElements(): Double {
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 100L) {
            val outer: MutableList<MutableList<Long>> = mutableListOf()
            run {
                var j: Long = 0L
                while (j < 3L) {
                    outer.add(mutableListOf())
                    outer[j.toInt()].add(i + j)
                    j = j + 1L
                }
            }
            total = total + outer.size.toLong() + outer[2].size.toLong() + outer[1][0] - i
            i = i + 1L
        }
    }
    return offInt(total, 100L * (3L + 1L + 1L))
}

fun firstOf(xs: MutableList<MutableList<Long>>): MutableList<Long> {
    val first: MutableList<Long> = xs[0]
    return first
}

fun checkAnElementReadOutOfAListIsNotReleasedWithIt(): Double {
    val store: MutableList<MutableList<Long>> = mutableListOf()
    store.add(mutableListOf())
    store[0].add(6L)
    val got: MutableList<Long> = firstOf(store)
    got.add(7L)
    return offInt(store[0].size.toLong(), 2L) + offInt(got[1], 7L)
}

fun checkAMapPastItsStartingCapacityKeepsEveryEntry(): Double {
    val m: MutableMap<Long, Long> = mutableMapOf()
    run {
        var i: Long = 0L
        while (i < 1000L) {
            m[i] = i * 3L
            i = i + 1L
        }
    }
    var wrong: Long = 0L
    run {
        var i: Long = 0L
        while (i < 1000L) {
            if (m[i] ?: 0L != i * 3L) {
                wrong = wrong + 1L
            }
            i = i + 1L
        }
    }
    return offInt(wrong, 0L) + offInt(m.size.toLong(), 1000L)
}

fun checkAGrownMapIsStillTheSameMapForItsAliases(): Double {
    val m: MutableMap<Long, Long> = mutableMapOf()
    val alias: MutableMap<Long, Long> = m
    run {
        var i: Long = 0L
        while (i < 20L) {
            alias[i] = i
            i = i + 1L
        }
    }
    return offInt(m.size.toLong(), 20L) + offInt(m[19L] ?: 0L, 19L)
}

fun checkStringKeysSurviveGrowing(): Double {
    val m: MutableMap<String, Long> = mutableMapOf()
    m["a"] = 1L
    m["b"] = 2L
    m["c"] = 3L
    m["d"] = 4L
    m["a"] = 10L
    return offInt(m["a"] ?: 0L, 10L) + offInt(m["b"] ?: 0L, 2L) + offInt(m["c"] ?: 0L, 3L) + offInt(m["d"] ?: 0L, 4L) + offInt(m.size.toLong(), 4L)
}

data class GrowKey(val a: Long, val b: Long)

fun checkStructKeysSurviveGrowing(): Double {
    val m: MutableMap<GrowKey, Long> = mutableMapOf()
    run {
        var i: Long = 0L
        while (i < 50L) {
            m[GrowKey(i, i + 1L)] = i
            i = i + 1L
        }
    }
    var wrong: Long = 0L
    run {
        var i: Long = 0L
        while (i < 50L) {
            if (m[GrowKey(i, i + 1L)] ?: 0L != i) {
                wrong = wrong + 1L
            }
            i = i + 1L
        }
    }
    return offInt(wrong, 0L) + offInt(m.size.toLong(), 50L)
}

fun checkASetPastItsStartingCapacityKeepsEveryMember(): Double {
    val seen: MutableSet<Long> = mutableSetOf()
    run {
        var i: Long = 0L
        while (i < 100L) {
            seen.add(i % 40L)
            i = i + 1L
        }
    }
    var absent: Long = 0L
    if (seen.contains(39L) == false) {
        absent = absent + 1L
    }
    if (seen.contains(40L)) {
        absent = absent + 1L
    }
    return offInt(seen.size.toLong(), 40L) + offInt(absent, 0L)
}

fun readAt(xs: MutableList<Long>, i: Long): Long {
    return xs[i.toInt()]
}

fun caughtCode(xs: MutableList<Long>, i: Long): Long {
    var code: Long = 0L
    try {
        code = readAt(xs, i)
    } catch (thrown: Thrown) {
        val c: Long = thrown.value
        code = c
    }
    return code
}

fun checkAnIndexPastTheEndOfAListThrowsMinusOne(): Double {
    val xs: MutableList<Long> = mutableListOf()
    xs.add(5L)
    return offInt(caughtCode(xs, 1L), 0L - 1L) + offInt(caughtCode(xs, 100000L), 0L - 1L)
}

fun checkANegativeIndexThrowsMinusOne(): Double {
    val xs: MutableList<Long> = mutableListOf()
    xs.add(5L)
    return offInt(caughtCode(xs, 0L - 1L), 0L - 1L)
}

fun checkAnIndexInsideTheListStillReads(): Double {
    val xs: MutableList<Long> = mutableListOf()
    xs.add(5L)
    xs.add(6L)
    return offInt(caughtCode(xs, 0L), 5L) + offInt(caughtCode(xs, 1L), 6L)
}

fun checkAnEmptyListCannotBeRead(): Double {
    val xs: MutableList<Long> = mutableListOf()
    return offInt(caughtCode(xs, 0L), 0L - 1L)
}

fun divideBy(a: Long, b: Long): Long {
    var out: Long = 0L
    try {
        out = a / b
    } catch (thrown: Thrown) {
        val c: Long = thrown.value
        out = c
    }
    return out
}

fun checkDividingByZeroThrowsMinusFour(): Double {
    return offInt(divideBy(10L, 0L), 0L - 4L) + offInt(divideBy(10L, 2L), 5L)
}

fun checkAFixedArrayAndAGridAreCheckedPerDimension(): Double {
    var code: Long = 0L
    val g: Grid<Long> = Grid(listOf(2, 2), 0L)
    try {
        g.set(1L, 0L, 5L)
    } catch (thrown: Thrown) {
        val c: Long = thrown.value
        code = c
    }
    return offInt(code, 0L - 1L)
}

data class OptPoint(val x: Long, val y: Long)

fun optionalFind(xs: MutableList<Long>, want: Long): Long? {
    run {
        var i: Long = 0L
        while (i < xs.size.toLong()) {
            if (xs[i.toInt()] == want) {
                return i
            }
            i = i + 1L
        }
    }
    return null
}

fun optionalPoint(ok: Long): OptPoint? {
    if (ok == 1L) {
        return OptPoint(7L, 8L)
    }
    return null
}

fun optionalName(ok: Long): String? {
    if (ok == 1L) {
        return "ann"
    }
    return null
}

fun unwrapCode(o: Long?): Long {
    var code: Long = 0L
    try {
        code = o ?: fail("unwrap of an empty optional")
    } catch (thrown: Thrown) {
        val c: Long = thrown.value
        code = c
    }
    return code
}

fun checkAnOptionalHoldsAValueOrNone(): Double {
    val xs: MutableList<Long> = mutableListOf()
    xs.add(10L)
    xs.add(20L)
    var seen: Long = 0L
    if (optionalFind(xs, 20L) != null) {
        seen = seen + 1L
    }
    if (optionalFind(xs, 99L) == null) {
        seen = seen + 10L
    }
    return offInt(seen, 11L) + offInt(optionalFind(xs, 20L) ?: fail("unwrap of an empty optional"), 1L)
}

fun checkAnOptionalOfAStructAndOfAStringWork(): Double {
    val p: OptPoint? = optionalPoint(1L)
    var seen: Long = 0L
    if (p != null) {
        seen = seen + (p ?: fail("unwrap of an empty optional")).x + (p ?: fail("unwrap of an empty optional")).y
    }
    if (optionalPoint(0L) == null) {
        seen = seen + 100L
    }
    if (optionalName(1L) != null) {
        seen = seen + 1000L
    }
    return offInt(seen, 1115L)
}

fun checkUnwrappingNoneThrowsMinusThree(): Double {
    return offInt(unwrapCode(null), 0L - 3L) + offInt(unwrapCode(42L), 42L)
}
