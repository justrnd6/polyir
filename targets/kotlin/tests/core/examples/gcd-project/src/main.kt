import kotlin.system.exitProcess

fun main(): Long {
    return gcd(48L, 18L) * 10L + lcm(4L, 6L)
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
