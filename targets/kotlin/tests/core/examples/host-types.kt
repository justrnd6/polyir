import kotlin.system.exitProcess

interface Runnable {
    fun run(): Long
}

data class Job(val id: Long) : Runnable {
    override fun run(): Long {
        return this.id
    }
}

fun main(): Long {
    return 0L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
