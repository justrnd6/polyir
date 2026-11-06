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

data class Counter(var n: Long, val id: Long) {
    fun bump(): Long {
        this.n = this.n + 1L
        return this.n
    }
}

data class Pt(val x: Long, val y: Long)
data class Holder(var items: MutableList<Long>, var inner: Counter)

interface Reading {
    fun read(): Long
}

data class Gauge(var level: Long) : Reading {
    override fun read(): Long {
        return this.level
    }
}

fun addTen(c: Counter): Long {
    c.n = c.n + 10L
    return 0L
}

fun addTenOnACopy(c: Counter): Long {
    val c: Counter = c.copy()
    c.n = c.n + 10L
    return c.n
}

fun checkTwoNamesForOneObjectShareIt(): Double {
    val a: Counter = Counter(1L, 7L)
    val b: Counter = a
    b.n = 5L
    return offInt(a.n, 5L) + offInt(a.id, 7L)
}

fun checkACalleeChangesTheCallersObject(): Double {
    val a: Counter = Counter(1L, 0L)
    addTen(a)
    return offInt(a.n, 11L)
}

fun checkAValParameterIsTheCalleesOwnCopy(): Double {
    val a: Counter = Counter(1L, 0L)
    val seen: Long = addTenOnACopy(a)
    return offInt(seen, 11L) + offInt(a.n, 1L)
}

fun checkValInFrontOfAnExpressionCopies(): Double {
    val a: Counter = Counter(1L, 3L)
    val b: Counter = a.copy()
    b.n = 50L
    return offInt(a.n, 1L) + offInt(b.n, 50L) + offInt(b.id, 3L)
}

fun checkCopyMethodAndValAgree(): Double {
    val a: Counter = Counter(4L, 0L)
    val b: Counter = a.copy()
    a.n = 9L
    return offInt(b.n, 4L)
}

fun checkAMethodChangesThis(): Double {
    val a: Counter = Counter(0L, 0L)
    a.bump()
    a.bump()
    return offInt(a.n, 2L)
}

fun checkAnImmutableStructIsAValue(): Double {
    val p: Pt = Pt(1L, 2L)
    val q: Pt = p.copy()
    val r: Pt = p.copy()
    return offInt(q.x + r.y, 3L)
}

fun checkAFieldHoldingAnotherReferenceStruct(): Double {
    val h: Holder = Holder(mutableListOf(), Counter(1L, 0L))
    val alias: Holder = h
    alias.inner.n = 8L
    alias.items.add(5L)
    return offInt(h.inner.n, 8L) + offInt(h.items.size.toLong(), 1L)
}

fun checkAFieldCanBeReplacedWithAnotherObject(): Double {
    val h: Holder = Holder(mutableListOf(), Counter(1L, 0L))
    val keep: Counter = h.inner
    h.inner = Counter(2L, 0L)
    return offInt(keep.n, 1L) + offInt(h.inner.n, 2L)
}

fun checkAGenericStructWithAVarField(): Double {
    val c: Cell<Long> = Cell(1L)
    val d: Cell<Long> = c
    d.v = 9L
    return offInt(c.v, 9L)
}

fun checkAnInterfaceValueSeesLaterChanges(): Double {
    val g: Gauge = Gauge(1L)
    val r: Reading = g
    g.level = 42L
    return offInt(r.read(), 42L)
}

fun checkAStructInADarrayIsTheSameObject(): Double {
    val all: MutableList<Counter> = mutableListOf()
    val a: Counter = Counter(1L, 0L)
    all.add(a)
    a.n = 6L
    return offInt(all[0].n, 6L)
}

fun checkAStructInAClosureIsShared(): Double {
    val a: Counter = Counter(1L, 0L)
    val f: () -> Long = fun(): Long {
        a.n = a.n + 1L
        return a.n
    }
    f()
    return offInt(a.n, 2L)
}

fun makeCounter(start: Long): Counter {
    val c: Counter = Counter(start, 1L)
    c.n = c.n + 1L
    return c
}

fun lend(c: Counter): Long {
    return c.n * 2L
}

fun keepIn(store: MutableList<Counter>, c: Counter): Long {
    store.add(c)
    return 0L
}

fun checkAReturnedObjectSurvives(): Double {
    val a: Counter = makeCounter(5L)
    val b: Counter = makeCounter(10L)
    return offInt(a.n * 100L + b.n, 611L)
}

fun checkAnObjectLentToACalleeIsUsableAfterwards(): Double {
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 100L) {
            val c: Counter = Counter(i, 0L)
            total = total + lend(c) + c.n
            c.n = 0L
            i = i + 1L
        }
    }
    return offInt(total, 3L * 4950L)
}

fun checkAnObjectAnotherCalleeKeepsSurvives(): Double {
    val store: MutableList<Counter> = mutableListOf()
    run {
        var i: Long = 0L
        while (i < 4L) {
            val c: Counter = Counter(i * 10L, 0L)
            keepIn(store, c)
            i = i + 1L
        }
    }
    return offInt(store[0].n + store[1].n + store[2].n + store[3].n, 60L)
}

fun checkAValParameterCopyIsReleasedAndTheOriginalIsNot(): Double {
    val a: Counter = Counter(1L, 0L)
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 50L) {
            total = total + addTenOnACopy(a)
            i = i + 1L
        }
    }
    return offInt(total, 50L * 11L) + offInt(a.n, 1L)
}

fun earlyReturnWithObjects(n: Long): Long {
    val c: Counter = Counter(n, 0L)
    if (n > 5L) {
        return c.n * 2L
    }
    c.bump()
    return c.n
}

fun checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue(): Double {
    return offInt(earlyReturnWithObjects(9L) * 100L + earlyReturnWithObjects(2L), 1800L + 3L)
}

fun checkLetVarAndValOnReferenceStructs(): Double {
    val a: Counter = Counter(1L, 0L)
    val alias: Counter = a
    val copy: Counter = a.copy()
    var moving: Counter = a
    a.n = 10L
    moving = Counter(100L, 0L)
    moving.n = moving.n + 1L
    return offInt(alias.n, 10L) + offInt(copy.n, 1L) + offInt(moving.n, 101L) + offInt(a.n, 10L)
}

fun checkValOnAContainerIsAnIndependentCopy(): Double {
    val d: MutableList<Long> = mutableListOf()
    d.add(4L)
    val e: MutableList<Long> = d.toMutableList()
    d.add(5L)
    return offInt(e.size.toLong(), 1L) + offInt(d.size.toLong(), 2L)
}

data class Dot(val x: Long, val y: Long)

data class Span(val lo: Long, val hi: Long) {
    fun width(): Long {
        return this.hi - this.lo
    }
}

data class Meter(var reading: Long, val id: Long) {
    fun bump(): Long {
        this.reading = this.reading + 1L
        return this.reading
    }
}

data class Plate(val code: Long) : Reading {
    override fun read(): Long {
        return this.code
    }
}

fun checkShorthandStructsWork(): Double {
    val d: Dot = Dot(3L, 4L)
    val s: Span = Span(2L, 9L)
    val w: Wrapper<Long> = Wrapper(5L)
    val r: Reading = Plate(7L)
    return offInt(d.x + d.y, 7L) + offInt(s.width(), 7L) + offInt(w.value, 5L) + offInt(r.read(), 7L)
}

fun checkAShorthandStructWithAVarFieldIsAReference(): Double {
    val m: Meter = Meter(1L, 2L)
    val alias: Meter = m
    alias.bump()
    return offInt(m.reading, 2L) + offInt(m.id, 2L)
}

fun checkAHashmapOfObjectsIsUsableThenReleased(): Double {
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 100L) {
            val m: MutableMap<Long, Counter> = mutableMapOf()
            m[1L] = Counter(i, 1L)
            m[2L] = Counter(5L, 2L)
            m[2L] = Counter(6L, 2L)
            (m[1L] ?: null).bump()
            total = total + (m[1L] ?: null).n - i + (m[2L] ?: null).n + m.size.toLong()
            i = i + 1L
        }
    }
    return offInt(total, 100L * (1L + 6L + 2L))
}

fun checkASparseOfStringsIsUsableThenReleased(): Double {
    var total: Long = 0L
    run {
        var i: Long = 0L
        while (i < 100L) {
            val s: Sparse<String> = Sparse("")
            s.set("a" + "b", 0L, 0L)
            s.set("c" + "d", 2L, 3L)
            if (s.get(2L, 3L) == "cd") {
                total = total + s.size.toLong()
            }
            i = i + 1L
        }
    }
    return offInt(total, 200L)
}

data class Cell<T>(var v: T)
data class Wrapper<T>(val value: T)
