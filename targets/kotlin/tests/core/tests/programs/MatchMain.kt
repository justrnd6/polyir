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
    failed = failed + failedIf(checkComparisons())
    failed = failed + failedIf(checkGuardsOnTheSubject())
    failed = failed + failedIf(checkStrings())
    failed = failed + failedIf(checkConditions())
    failed = failed + failedIf(checkBooleans())
    failed = failed + failedIf(checkOptionals())
    failed = failed + failedIf(checkEithers())
    failed = failed + failedIf(checkVariants())
    failed = failed + failedIf(checkPayloadPatterns())
    failed = failed + failedIf(checkLists())
    failed = failed + failedIf(checkListGuards())
    failed = failed + failedIf(checkStatements())
    failed = failed + failedIf(checkNested())
    return failed
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
