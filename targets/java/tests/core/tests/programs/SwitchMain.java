final class SwitchMain {
    static long failedIf(double off) {
        if (off > 0.001 || off < 0.0 - 0.001) {
            return 1L;
        }
        return 0L;
    }

    static long main() {
        long failed = 0L;
        failed = failed + failedIf(SwitchTests.checkNumbers());
        failed = failed + failedIf(SwitchTests.checkChars());
        failed = failed + failedIf(SwitchTests.checkStrings());
        failed = failed + failedIf(SwitchTests.checkBooleans());
        failed = failed + failedIf(SwitchTests.checkSums());
        failed = failed + failedIf(SwitchTests.checkConditionsAmongKeys());
        failed = failed + failedIf(SwitchTests.checkCallSubject());
        failed = failed + failedIf(SwitchTests.checkConditions());
        return failed;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
