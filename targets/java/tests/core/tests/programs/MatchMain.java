final class MatchMain {
    static long failedIf(double off) {
        if (off > 0.001 || off < 0.0 - 0.001) {
            return 1L;
        }
        return 0L;
    }

    static long main() {
        long failed = 0L;
        failed = failed + failedIf(MatchTests.checkNumbers());
        failed = failed + failedIf(MatchTests.checkComparisons());
        failed = failed + failedIf(MatchTests.checkGuardsOnTheSubject());
        failed = failed + failedIf(MatchTests.checkStrings());
        failed = failed + failedIf(MatchTests.checkConditions());
        failed = failed + failedIf(MatchTests.checkBooleans());
        failed = failed + failedIf(MatchTests.checkOptionals());
        failed = failed + failedIf(MatchTests.checkEithers());
        failed = failed + failedIf(MatchTests.checkVariants());
        failed = failed + failedIf(MatchTests.checkPayloadPatterns());
        failed = failed + failedIf(MatchTests.checkLists());
        failed = failed + failedIf(MatchTests.checkListGuards());
        failed = failed + failedIf(MatchTests.checkStatements());
        failed = failed + failedIf(MatchTests.checkNested());
        return failed;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
