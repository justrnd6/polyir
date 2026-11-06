import kotlin.system.exitProcess

// What `throw` throws: the code thrown.
class Thrown(val value: Long) : RuntimeException("thrown $value")

data class Acc(var total: Long, var tag: String, var items: MutableList<Long>) {
    fun add(x: Long): Long {
        this.total = this.total + x
        this.items.add(x)
        return this.total
    }

    fun later(): () -> Long {
        return { this.total * 2L }
    }

    fun describe(): String {
        return this.tag + ":" + this.tag
    }

    fun self(): Acc {
        return this
    }
}

interface Animal {
    fun legs(p0: Long): Long
    fun name(): String
}

data class Dog(val age: Long) : Animal {
    override fun legs(n: Long): Long {
        return 4L * n + this.age
    }

    override fun name(): String {
        return "dog"
    }
}

data class Bird(val age: Long) : Animal {
    override fun legs(n: Long): Long {
        return 2L * n
    }

    override fun name(): String {
        return "bird"
    }
}

fun risky(x: Long): Long {
    if (x < 0L) {
        throw Thrown(7L)
    }
    return x
}

fun tryReturn(x: Long): Long {
    try {
        return risky(x) + 1L
    } catch (thrown: Thrown) {
        val code: Long = thrown.value
        return code * 100L
    }
    return 0L
}

fun nested(x: Long): Long {
    var r: Long = 0L
    try {
        try {
            r = risky(x)
        } catch (thrown: Thrown) {
            val a: Long = thrown.value
            r = a + 1L
            throw Thrown(r)
        }
    } catch (thrown: Thrown) {
        val b: Long = thrown.value
        r = b * 10L
    }
    return r
}

fun makeAdder(n: Long): (Long) -> Long {
    return { x -> x + n }
}

fun twice(f: (Long) -> Long, x: Long): Long {
    return f(f(x))
}

fun counterPair(): Long {
    var c: Long = 0L
    val inc: () -> Long = fun(): Long {
        c = c + 1L
        return c
    }
    val get: () -> Long = { c }
    inc()
    inc()
    return get() * 10L + c
}

fun main(): Long {
    var score: Long = 0L
    val a: Acc = Acc(1L, "ab", mutableListOf())
    score = score + a.add(5L) + a.add(6L)
    val f: () -> Long = a.later()
    score = score + f()
    score = score + a.items.size.toLong()
    if (a.describe() == "ab:ab") {
        score = score + 1000L
    }
    val s: Acc = a.self()
    if (s === a) {
        score = score + 2000L
    }
    a.tag = "zz"
    if (s.tag == "zz") {
        score = score + 3000L
    }
    val zoo: MutableList<Animal> = mutableListOf()
    zoo.add(Dog(3L))
    zoo.add(Bird(1L))
    run {
        var i: Long = 0L
        while (i < zoo.size.toLong()) {
            score = score + zoo[i.toInt()].legs(2L)
            if (zoo[i.toInt()].name() == "bird") {
                score = score + 7L
            }
            i = i + 1L
        }
    }
    score = score + tryReturn(3L) + tryReturn(0L - 1L)
    score = score + nested(4L) + nested(0L - 2L)
    val add3: (Long) -> Long = makeAdder(3L)
    score = score + twice(add3, 10L)
    score = score + counterPair()
    return score % 251L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
