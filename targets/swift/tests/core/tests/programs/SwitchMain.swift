import Foundation

func failedIf(_ off: Double) -> Int64 {
    if off > 0.001 || off < 0.0 - 0.001 {
        return 1
    }
    return 0
}

func main() -> Int64 {
    var failed: Int64 = 0
    failed = failed &+ failedIf(checkNumbers())
    failed = failed &+ failedIf(checkChars())
    failed = failed &+ failedIf(checkStrings())
    failed = failed &+ failedIf(checkBooleans())
    failed = failed &+ failedIf(checkSums())
    failed = failed &+ failedIf(checkConditionsAmongKeys())
    failed = failed &+ failedIf(checkCallSubject())
    failed = failed &+ failedIf(checkConditions())
    return failed
}

exit(Int32(truncatingIfNeeded: main()))
