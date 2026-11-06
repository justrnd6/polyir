import kotlin.system.exitProcess

fun main(): Long {
    val gpu: Long = vulkanStart()
    if (gpu == 0L) {
        return 10L
    }
    return runAll(gpu)
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
