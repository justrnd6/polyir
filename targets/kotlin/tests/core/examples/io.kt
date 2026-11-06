import kotlin.system.exitProcess

private fun putchar(c: Int): Int {
    System.out.write(c.toInt())
    System.out.flush()
    return c
}

private fun getchar(): Int = System.`in`.read().toInt()

fun writeLine(): Int {
    putchar(104)
    putchar(108)
    putchar(108)
    putchar(118)
    putchar(109)
    putchar(32)
    putchar(105)
    putchar(111)
    return putchar(10)
}

fun echoOneByte(): Int {
    val c: Int = getchar()
    return putchar(c)
}

fun main(): Long {
    writeLine()
    echoOneByte()
    return 0L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
