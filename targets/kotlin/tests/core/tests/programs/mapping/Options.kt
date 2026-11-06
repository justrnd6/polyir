import kotlin.system.exitProcess

// Stops the program with a message, as polyir's `fail` does.
private fun fail(message: String): Nothing {
    System.err.println("trap: " + message)
    exitProcess(1)
}

data class Pt(val x: Long, val y: Long)
data class Contact(val name: String, val nickname: String?)

fun find(xs: MutableList<Long>, want: Long): Long? {
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

fun firstPoint(ok: Long): Pt? {
    if (ok == 1L) {
        return Pt(7L, 8L)
    }
    return null
}

fun shown(c: Contact): String {
    if (c.nickname != null) {
        return c.nickname ?: fail("unwrap of an empty optional")
    }
    return c.name
}

fun main(): Long {
    val xs: MutableList<Long> = mutableListOf()
    xs.add(10L)
    xs.add(20L)
    var score: Long = 0L
    val hit: Long? = find(xs, 20L)
    if (hit != null) {
        score = score + (hit ?: fail("unwrap of an empty optional"))
    }
    if (find(xs, 99L) == null) {
        score = score + 10L
    }
    val p: Pt? = firstPoint(1L)
    if (p != null) {
        score = score + (p ?: fail("unwrap of an empty optional")).x
    }
    if (firstPoint(0L) == null) {
        score = score + 100L
    }
    val named: Contact = Contact("ann", "annie")
    val plain: Contact = Contact("bo", null)
    if (shown(named) == "annie") {
        score = score + 1000L
    }
    if (shown(plain) == "bo") {
        score = score + 2000L
    }
    return score
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
