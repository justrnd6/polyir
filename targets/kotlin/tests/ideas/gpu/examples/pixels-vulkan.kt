import kotlin.system.exitProcess

fun main(): Long {
    val gpu: Long = vulkanStart()
    if (gpu == 0L) {
        return 10L
    }
    printTable(facePicture())
    var code: Long = scanDemo(gpu, "")
    if (code != 0L) {
        return code
    }
    code = channelsDemo(gpu, "")
    if (code != 0L) {
        return code
    }
    code = bitsDemo(gpu, "")
    if (code != 0L) {
        return code
    }
    code = resizeDemo(gpu, "")
    if (code != 0L) {
        return code
    }
    return enlargeDemo(gpu, "")
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
