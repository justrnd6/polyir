import kotlin.system.exitProcess

interface Runnable {
    fun run(): Long
}

fun main(): Long {
    return 0L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
