import kotlin.system.exitProcess

fun main(): Long {
    val device: Long = metalDevice()
    if (device == 0L) {
        return 10L
    }
    val source: String = metalReadFile("camera.metal")
    if (source == "") {
        return 11L
    }
    return runAll(device, source)
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
