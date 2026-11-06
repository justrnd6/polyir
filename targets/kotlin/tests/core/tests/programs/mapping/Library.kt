import kotlin.system.exitProcess

var visits: Long = 0L

data class Account(var balance: Long, val owner: String) {
    fun deposit(amount: Long): Long {
        this.balance = this.balance + amount
        return this.balance
    }

    fun depositTwice(amount: Long): Long {
        this.deposit(amount)
        return this.deposit(amount)
    }
}

data class Vec2(val x: Double, val y: Double) {
    fun length2(): Double {
        return this.x * this.x + this.y * this.y
    }

    fun scaled(k: Double): Vec2 {
        return Vec2(this.x * k, this.y * k)
    }
}

fun fib(n: Long): Long {
    return if (n < 2L) n else fib(n - 1L) + fib(n - 2L)
}

fun classify(n: Long): Long {
    return if (n < 0L) 1L else (if (n == 0L) 2L else (if (n < 10L) 3L else 4L))
}

fun bump(): Long {
    visits = visits + 1L
    return visits
}

fun greeting(name: String): String {
    return "hello, " + name
}

fun narrow(a: Int, b: Int): Int {
    return a * b + 7
}

fun apply(f: (Long) -> Long, x: Long): Long {
    return f(x) + f(x + 1L)
}

fun main(): Long {
    var score: Long = 0L
    score = score + fib(10L)
    val acct: Account = Account(100L, "ann")
    score = score + acct.depositTwice(5L)
    val v: Vec2 = Vec2(3.0, 4.0)
    val w: Vec2 = v.scaled(2.0)
    score = score + (w.length2().toLong())
    if (greeting("bo") == "hello, bo") {
        score = score + 1L
    }
    score = score + (narrow(6, 7).toLong())
    val seen: MutableSet<Long> = mutableSetOf()
    run {
        var i: Long = 0L
        while (i < 20L) {
            seen.add(i % 7L)
            i = i + 1L
        }
    }
    score = score + seen.size.toLong()
    val nums: MutableList<Long> = mutableListOf()
    run {
        var i: Long = 0L
        var j: Long = 10L
        while (i < 5L) {
            nums.add(i * j)
            i = i + 1L
            j = j - 1L
        }
    }
    nums[0] = 100L
    score = score + nums[0] + nums[4] + nums.size.toLong()
    val ages: MutableMap<String, Long> = mutableMapOf()
    ages["ann"] = 31L
    if (ages.containsKey("ann")) {
        score = score + (ages["ann"] ?: 0L)
    }
    if (ages.containsKey("zed") == false) {
        score = score + 1L
    }
    val factor: Long = 3L
    score = score + apply({ x -> x * factor }, 4L)
    val b: Box<Double> = Box(2.5)
    score = score + ((b.value * 4.0).toLong())
    score = score + classify(0L - 5L) + classify(0L) + classify(7L) + classify(70L)
    score = score + bump() + bump()
    return score
}

data class Box<T>(val value: T)

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
