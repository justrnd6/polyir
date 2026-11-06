import kotlin.system.exitProcess

fun main(): Long {
    val device: Long = metalDevice()
    if (device == 0L) {
        return 10L
    }
    val source: String = metalReadFile("pixels.metal")
    if (source == "") {
        return 11L
    }
    printTable(facePicture())
    var code: Long = scanDemo(device, source)
    if (code != 0L) {
        return code
    }
    code = channelsDemo(device, source)
    if (code != 0L) {
        return code
    }
    code = bitsDemo(device, source)
    if (code != 0L) {
        return code
    }
    code = resizeDemo(device, source)
    if (code != 0L) {
        return code
    }
    return enlargeDemo(device, source)
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
