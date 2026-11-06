import kotlin.system.exitProcess

fun failedIf(off: Double): Long {
    if (off > 0.001 || off < 0.0 - 0.001) {
        return 1L
    }
    return 0L
}

fun main(): Long {
    var failed: Long = 0L
    failed = failed + failedIf(checkNumbers())
    failed = failed + failedIf(checkChars())
    failed = failed + failedIf(checkStrings())
    failed = failed + failedIf(checkBooleans())
    failed = failed + failedIf(checkSums())
    failed = failed + failedIf(checkConditionsAmongKeys())
    failed = failed + failedIf(checkCallSubject())
    failed = failed + failedIf(checkConditions())
    return failed
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
